/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/Timer.jsm');
Cu.import('resource://gre/modules/Promise.jsm');
Cu.import('resource://gre/modules/Preferences.jsm');

const serviceExports = Cu.import('resource://gre/modules/PushService.jsm', {});
const servicePrefs = new Preferences('dom.push.');

XPCOMUtils.defineLazyServiceGetter(
  this,
  "PushNotificationService",
  "@mozilla.org/push/NotificationService;1",
  "nsIPushNotificationService"
);

const DEFAULT_TIMEOUT = 5000;

const WEBSOCKET_CLOSE_GOING_AWAY = 1001;

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
 * Waits for a promise to settle. Returns a rejected promise if the promise
 * is not resolved or rejected within the given delay.
 *
 * @param {Promise} promise The pending promise.
 * @param {Number} delay The time to wait before rejecting the promise.
 * @param {String} [message] The rejection message if the promise times out.
 * @returns {Promise} A promise that settles with the value of the pending
 *  promise, or rejects if the pending promise times out.
 */
function waitForPromise(promise, delay, message = 'Timed out waiting on promise') {
  let timeoutDefer = Promise.defer();
  let id = setTimeout(() => timeoutDefer.reject(new Error(message)), delay);
  return Promise.race([
    promise.then(value => {
      clearTimeout(id);
      return value;
    }, error => {
      clearTimeout(id);
      throw error;
    }),
    timeoutDefer.promise
  ]);
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
 * Disables `push` and `pushsubscriptionchange` service worker events for the
 * given scopes. These events cause crashes in xpcshell, so we disable them
 * for testing nsIPushNotificationService.
 *
 * @param {String[]} scopes A list of scope URLs.
 */
function disableServiceWorkerEvents(...scopes) {
  for (let scope of scopes) {
    Services.perms.add(
      Services.io.newURI(scope, null, null),
      'push',
      Ci.nsIPermissionManager.DENY_ACTION
    );
  }
}

/**
 * Sets default PushService preferences. All pref names are prefixed with
 * `dom.push.`; any additional preferences will override the defaults.
 *
 * @param {Object} [prefs] Additional preferences to set.
 */
function setPrefs(prefs = {}) {
  let defaultPrefs = Object.assign({
    debug: true,
    serverURL: 'wss://push.example.org',
    'connection.enabled': true,
    userAgentID: '',
    enabled: true,
    // Disable adaptive pings and UDP wake-up by default; these are
    // tested separately.
    'adaptive.enabled': false,
    'udp.wakeupEnabled': false,
    // Defaults taken from /b2g/app/b2g.js.
    requestTimeout: 10000,
    retryBaseInterval: 5000,
    pingInterval: 30 * 60 * 1000,
    'pingInterval.default': 3 * 60 * 1000,
    'pingInterval.mobile': 3 * 60 * 1000,
    'pingInterval.wifi': 3 * 60 * 1000,
    'adaptive.lastGoodPingInterval': 3 * 60 * 1000,
    'adaptive.lastGoodPingInterval.mobile': 3 * 60 * 1000,
    'adaptive.lastGoodPingInterval.wifi': 3 * 60 * 1000,
    'adaptive.gap': 60000,
    'adaptive.upperLimit': 29 * 60 * 1000,
    // Misc. defaults.
    'adaptive.mobile': ''
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

  asyncOpen(uri, origin, listener, context) {
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
   * followed by onStop(). Used to test abrupt connection termination
   * and UDP wake-up.
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
  }
};

/**
 * Creates an object that exposes the same interface as NetworkInfo, used
 * to simulate network status changes on Desktop. All methods returns empty
 * carrier data.
 */
function MockDesktopNetworkInfo() {}

MockDesktopNetworkInfo.prototype = {
  getNetworkInformation() {
    return {mcc: '', mnc: '', ip: ''};
  },

  getNetworkState(callback) {
    callback({mcc: '', mnc: '', ip: '', netid: ''});
  },

  getNetworkStateChangeEventName() {
    return 'network:offline-status-changed';
  }
};

/**
 * Creates an object that exposes the same interface as NetworkInfo, used
 * to simulate network status changes on B2G.
 *
 * @param {String} [info.mcc] The mobile country code.
 * @param {String} [info.mnc] The mobile network code.
 * @param {String} [info.ip] The carrier IP address.
 * @param {String} [info.netid] The resolved network ID for UDP wake-up.
 */
function MockMobileNetworkInfo(info = {}) {
  this._info = info;
}

MockMobileNetworkInfo.prototype = {
  _info: null,

  getNetworkInformation() {
    let {mcc, mnc, ip} = this._info;
    return {mcc, mnc, ip};
  },

  getNetworkState(callback) {
    let {mcc, mnc, ip, netid} = this._info;
    callback({mcc, mnc, ip, netid});
  },

  getNetworkStateChangeEventName() {
    return 'network-active-changed';
  }
};
