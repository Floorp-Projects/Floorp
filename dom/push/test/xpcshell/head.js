/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/Task.jsm');
Cu.import('resource://gre/modules/Timer.jsm');
Cu.import('resource://gre/modules/Promise.jsm');
Cu.import('resource://gre/modules/Preferences.jsm');
Cu.import('resource://gre/modules/PlacesUtils.jsm');
Cu.import('resource://gre/modules/ObjectUtils.jsm');

XPCOMUtils.defineLazyModuleGetter(this, 'PlacesTestUtils',
                                  'resource://testing-common/PlacesTestUtils.jsm');
XPCOMUtils.defineLazyServiceGetter(this, 'PushServiceComponent',
                                   '@mozilla.org/push/Service;1', 'nsIPushService');

const serviceExports = Cu.import('resource://gre/modules/PushService.jsm', {});
const servicePrefs = new Preferences('dom.push.');

const WEBSOCKET_CLOSE_GOING_AWAY = 1001;

const MS_IN_ONE_DAY = 24 * 60 * 60 * 1000;

var isParent = Cc['@mozilla.org/xre/runtime;1']
                 .getService(Ci.nsIXULRuntime).processType ==
                 Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

// Stop and clean up after the PushService.
Services.obs.addObserver(function observe(subject, topic, data) {
  Services.obs.removeObserver(observe, topic, false);
  serviceExports.PushService.uninit();
  // Occasionally, `profile-change-teardown` and `xpcom-shutdown` will fire
  // before the PushService and AlarmService finish writing to IndexedDB. This
  // causes spurious errors and crashes, so we spin the event loop to let the
  // writes finish.
  let done = false;
  setTimeout(() => done = true, 1000);
  let thread = Services.tm.mainThread;
  while (!done) {
    try {
      thread.processNextEvent(true);
    } catch (e) {
      Cu.reportError(e);
    }
  }
}, 'profile-change-net-teardown', false);

/**
 * Gates a function so that it is called only after the wrapper is called a
 * given number of times.
 *
 * @param {Number} times The number of wrapper calls before |func| is called.
 * @param {Function} func The function to gate.
 * @returns {Function} The gated function wrapper.
 */
function after(times, func) {
  return function afterFunc() {
    if (--times <= 0) {
      return func.apply(this, arguments);
    }
  };
}

/**
 * Defers one or more callbacks until the next turn of the event loop. Multiple
 * callbacks are executed in order.
 *
 * @param {Function[]} callbacks The callbacks to execute. One callback will be
 *  executed per tick.
 */
function waterfall(...callbacks) {
  callbacks.reduce((promise, callback) => promise.then(() => {
    callback();
  }), Promise.resolve()).catch(Cu.reportError);
}

/**
 * Waits for an observer notification to fire.
 *
 * @param {String} topic The notification topic.
 * @returns {Promise} A promise that fulfills when the notification is fired.
 */
function promiseObserverNotification(topic, matchFunc) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic, data) {
      let matches = typeof matchFunc != 'function' || matchFunc(subject, data);
      if (!matches) {
        return;
      }
      Services.obs.removeObserver(observe, topic, false);
      resolve({subject, data});
    }, topic, false);
  });
}

/**
 * Wraps an object in a proxy that traps property gets and returns stubs. If
 * the stub is a function, the original value will be passed as the first
 * argument. If the original value is a function, the proxy returns a wrapper
 * that calls the stub; otherwise, the stub is called as a getter.
 *
 * @param {Object} target The object to wrap.
 * @param {Object} stubs An object containing stubbed values and functions.
 * @returns {Proxy} A proxy that returns stubs for property gets.
 */
function makeStub(target, stubs) {
  return new Proxy(target, {
    get(target, property) {
      if (!stubs || typeof stubs != 'object' || !(property in stubs)) {
        return target[property];
      }
      let stub = stubs[property];
      if (typeof stub != 'function') {
        return stub;
      }
      let original = target[property];
      if (typeof original != 'function') {
        return stub.call(this, original);
      }
      return function callStub(...params) {
        return stub.call(this, original, ...params);
      };
    }
  });
}

/**
 * Sets default PushService preferences. All pref names are prefixed with
 * `dom.push.`; any additional preferences will override the defaults.
 *
 * @param {Object} [prefs] Additional preferences to set.
 */
function setPrefs(prefs = {}) {
  let defaultPrefs = Object.assign({
    loglevel: 'all',
    serverURL: 'wss://push.example.org',
    'connection.enabled': true,
    userAgentID: '',
    enabled: true,
    // Defaults taken from /modules/libpref/init/all.js.
    requestTimeout: 10000,
    retryBaseInterval: 5000,
    pingInterval: 30 * 60 * 1000,
    // Misc. defaults.
    'http2.maxRetries': 2,
    'http2.retryInterval': 500,
    'http2.reset_retry_count_after_ms': 60000,
    maxQuotaPerSubscription: 16,
    quotaUpdateDelay: 3000,
    'testing.notifyWorkers': false,
  }, prefs);
  for (let pref in defaultPrefs) {
    servicePrefs.set(pref, defaultPrefs[pref]);
  }
}

function compareAscending(a, b) {
  return a > b ? 1 : a < b ? -1 : 0;
}

/**
 * Creates a mock WebSocket object that implements a subset of the
 * nsIWebSocketChannel interface used by the PushService.
 *
 * The given protocol handlers are invoked for each Simple Push command sent
 * by the PushService. The ping handler is optional; all others will throw if
 * the PushService sends a command for which no handler is registered.
 *
 * All nsIWebSocketListener methods will be called asynchronously.
 * serverSendMsg() and serverClose() can be used to respond to client messages
 * and close the "server" end of the connection, respectively.
 *
 * @param {nsIURI} originalURI The original WebSocket URL.
 * @param {Function} options.onHello The "hello" handshake command handler.
 * @param {Function} options.onRegister The "register" command handler.
 * @param {Function} options.onUnregister The "unregister" command handler.
 * @param {Function} options.onACK The "ack" command handler.
 * @param {Function} [options.onPing] An optional ping handler.
 */
function MockWebSocket(originalURI, handlers = {}) {
  this._originalURI = originalURI;
  this._onHello = handlers.onHello;
  this._onRegister = handlers.onRegister;
  this._onUnregister = handlers.onUnregister;
  this._onACK = handlers.onACK;
  this._onPing = handlers.onPing;
}

MockWebSocket.prototype = {
  _originalURI: null,
  _onHello: null,
  _onRegister: null,
  _onUnregister: null,
  _onACK: null,
  _onPing: null,

  _listener: null,
  _context: null,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISupports,
    Ci.nsIWebSocketChannel
  ]),

  get originalURI() {
    return this._originalURI;
  },

  asyncOpen(uri, origin, windowId, listener, context) {
    this._listener = listener;
    this._context = context;
    waterfall(() => this._listener.onStart(this._context));
  },

  _handleMessage(msg) {
    let messageType, request;
    if (msg == '{}') {
      request = {};
      messageType = 'ping';
    } else {
      request = JSON.parse(msg);
      messageType = request.messageType;
    }
    switch (messageType) {
    case 'hello':
      if (typeof this._onHello != 'function') {
        throw new Error('Unexpected handshake request');
      }
      this._onHello(request);
      break;

    case 'register':
      if (typeof this._onRegister != 'function') {
        throw new Error('Unexpected register request');
      }
      this._onRegister(request);
      break;

    case 'unregister':
      if (typeof this._onUnregister != 'function') {
        throw new Error('Unexpected unregister request');
      }
      this._onUnregister(request);
      break;

    case 'ack':
      if (typeof this._onACK != 'function') {
        throw new Error('Unexpected acknowledgement');
      }
      this._onACK(request);
      break;

    case 'ping':
      if (typeof this._onPing == 'function') {
        this._onPing(request);
      } else {
        // Echo ping packets.
        this.serverSendMsg('{}');
      }
      break;

    default:
      throw new Error('Unexpected message: ' + messageType);
    }
  },

  sendMsg(msg) {
    this._handleMessage(msg);
  },

  close(code, reason) {
    waterfall(() => this._listener.onStop(this._context, Cr.NS_OK));
  },

  /**
   * Responds with the given message, calling onMessageAvailable() and
   * onAcknowledge() synchronously. Throws if the message is not a string.
   * Used by the tests to respond to client commands.
   *
   * @param {String} msg The message to send to the client.
   */
  serverSendMsg(msg) {
    if (typeof msg != 'string') {
      throw new Error('Invalid response message');
    }
    waterfall(
      () => this._listener.onMessageAvailable(this._context, msg),
      () => this._listener.onAcknowledge(this._context, 0)
    );
  },

  /**
   * Closes the server end of the connection, calling onServerClose()
   * followed by onStop(). Used to test abrupt connection termination.
   *
   * @param {Number} [statusCode] The WebSocket connection close code.
   * @param {String} [reason] The connection close reason.
   */
  serverClose(statusCode, reason = '') {
    if (!isFinite(statusCode)) {
      statusCode = WEBSOCKET_CLOSE_GOING_AWAY;
    }
    waterfall(
      () => this._listener.onServerClose(this._context, statusCode, reason),
      () => this._listener.onStop(this._context, Cr.NS_BASE_STREAM_CLOSED)
    );
  },

  serverInterrupt(result = Cr.NS_ERROR_NET_RESET) {
    waterfall(() => this._listener.onStop(this._context, result));
  },
};

var setUpServiceInParent = Task.async(function* (service, db) {
  if (!isParent) {
    return;
  }

  let userAgentID = 'ce704e41-cb77-4206-b07b-5bf47114791b';
  setPrefs({
    userAgentID: userAgentID,
  });

  yield db.put({
    channelID: '6e2814e1-5f84-489e-b542-855cc1311f09',
    pushEndpoint: 'https://example.org/push/get',
    scope: 'https://example.com/get/ok',
    originAttributes: '',
    version: 1,
    pushCount: 10,
    lastPush: 1438360548322,
    quota: 16,
  });
  yield db.put({
    channelID: '3a414737-2fd0-44c0-af05-7efc172475fc',
    pushEndpoint: 'https://example.org/push/unsub',
    scope: 'https://example.com/unsub/ok',
    originAttributes: '',
    version: 2,
    pushCount: 10,
    lastPush: 1438360848322,
    quota: 4,
  });
  yield db.put({
    channelID: 'ca3054e8-b59b-4ea0-9c23-4a3c518f3161',
    pushEndpoint: 'https://example.org/push/stale',
    scope: 'https://example.com/unsub/fail',
    originAttributes: '',
    version: 3,
    pushCount: 10,
    lastPush: 1438362348322,
    quota: 1,
  });

  service.init({
    serverURI: 'wss://push.example.org/',
    db: makeStub(db, {
      put(prev, record) {
        if (record.scope == 'https://example.com/sub/fail') {
          return Promise.reject('synergies not aligned');
        }
        return prev.call(this, record);
      },
      delete: function(prev, channelID) {
        if (channelID == 'ca3054e8-b59b-4ea0-9c23-4a3c518f3161') {
          return Promise.reject('splines not reticulated');
        }
        return prev.call(this, channelID);
      },
      getByIdentifiers(prev, identifiers) {
        if (identifiers.scope == 'https://example.com/get/fail') {
          return Promise.reject('qualia unsynchronized');
        }
        return prev.call(this, identifiers);
      },
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            uaid: userAgentID,
            status: 200,
          }));
        },
        onRegister(request) {
          if (request.key) {
            let appServerKey = new Uint8Array(
              ChromeUtils.base64URLDecode(request.key, {
                padding: "require",
              })
            );
            equal(appServerKey.length, 65, 'Wrong app server key length');
            equal(appServerKey[0], 4, 'Wrong app server key format');
          }
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            uaid: userAgentID,
            channelID: request.channelID,
            status: 200,
            pushEndpoint: 'https://example.org/push/' + request.channelID,
          }));
        },
      });
    },
  });
});

var tearDownServiceInParent = Task.async(function* (db) {
  if (!isParent) {
    return;
  }

  let record = yield db.getByIdentifiers({
    scope: 'https://example.com/sub/ok',
    originAttributes: '',
  });
  ok(record.pushEndpoint.startsWith('https://example.org/push'),
    'Wrong push endpoint in subscription record');

  record = yield db.getByIdentifiers({
    scope: 'https://example.net/scope/1',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: 1, inIsolatedMozBrowser: true }),
  });
  ok(record.pushEndpoint.startsWith('https://example.org/push'),
    'Wrong push endpoint in app record');

  record = yield db.getByKeyID('3a414737-2fd0-44c0-af05-7efc172475fc');
  ok(!record, 'Unsubscribed record should not exist');
});

function putTestRecord(db, keyID, scope, quota) {
  return db.put({
    channelID: keyID,
    pushEndpoint: 'https://example.org/push/' + keyID,
    scope: scope,
    pushCount: 0,
    lastPush: 0,
    version: null,
    originAttributes: '',
    quota: quota,
    systemRecord: quota == Infinity,
  });
}

function getAllKeyIDs(db) {
  return db.getAllKeyIDs().then(records =>
    records.map(record => record.keyID).sort(compareAscending)
  );
}
