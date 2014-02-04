/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'stability': 'unstable'
};

const { Class } = require('../core/heritage');
const { EventTarget } = require('../event/target');
const { on, off, emit } = require('../event/core');
const {
  requiresAddonGlobal,
  attach, detach, destroy
} = require('./utils');
const { delay: async } = require('../lang/functional');
const { Ci, Cu, Cc } = require('chrome');
const timer = require('../timers');
const { URL } = require('../url');
const { sandbox, evaluate, load } = require('../loader/sandbox');
const { merge } = require('../util/object');
const xulApp = require('../system/xul-app');
const USE_JS_PROXIES = !xulApp.versionInRange(xulApp.platformVersion,
                                              '17.0a2', '*');
const { getTabForContentWindow } = require('../tabs/utils');

// WeakMap of sandboxes so we can access private values
const sandboxes = new WeakMap();

/* Trick the linker in order to ensure shipping these files in the XPI.
  require('./content-worker.js');
  Then, retrieve URL of these files in the XPI:
*/
let prefix = module.uri.split('sandbox.js')[0];
const CONTENT_WORKER_URL = prefix + 'content-worker.js';

// Fetch additional list of domains to authorize access to for each content
// script. It is stored in manifest `metadata` field which contains
// package.json data. This list is originaly defined by authors in
// `permissions` attribute of their package.json addon file.
const permissions = require('@loader/options').metadata['permissions'] || {};
const EXPANDED_PRINCIPALS = permissions['cross-domain-content'] || [];

const JS_VERSION = '1.8';

const WorkerSandbox = Class({

  implements: [
    EventTarget
  ],
  
  /**
   * Emit a message to the worker content sandbox
   */
  emit: function emit(...args) {
    // Ensure having an asynchronous behavior
    let self = this;
    async(function () {
      emitToContent(self, JSON.stringify(args, replacer));
    });
  },

  /**
   * Synchronous version of `emit`.
   * /!\ Should only be used when it is strictly mandatory /!\
   *     Doesn't ensure passing only JSON values.
   *     Mainly used by context-menu in order to avoid breaking it.
   */
  emitSync: function emitSync(...args) {
    return emitToContent(this, args);
  },

  /**
   * Tells if content script has at least one listener registered for one event,
   * through `self.on('xxx', ...)`.
   * /!\ Shouldn't be used. Implemented to avoid breaking context-menu API.
   */
  hasListenerFor: function hasListenerFor(name) {
    return modelFor(this).hasListenerFor(name);
  },

  /**
   * Configures sandbox and loads content scripts into it.
   * @param {Worker} worker
   *    content worker
   */
  initialize: function WorkerSandbox(worker, window) {
    let model = {};
    sandboxes.set(this, model);
    model.worker = worker;
    // We receive a wrapped window, that may be an xraywrapper if it's content
    let proto = window;

    // TODO necessary?
    // Ensure that `emit` has always the right `this`
    this.emit = this.emit.bind(this);
    this.emitSync = this.emitSync.bind(this);

    // Eventually use expanded principal sandbox feature, if some are given.
    //
    // But prevent it when the Worker isn't used for a content script but for
    // injecting `addon` object into a Panel, Widget, ... scope.
    // That's because:
    // 1/ It is useless to use multiple domains as the worker is only used
    // to communicate with the addon,
    // 2/ By using it it would prevent the document to have access to any JS
    // value of the worker. As JS values coming from multiple domain principals
    // can't be accessed by 'mono-principals' (principal with only one domain).
    // Even if this principal is for a domain that is specified in the multiple
    // domain principal.
    let principals = window;
    let wantGlobalProperties = [];
    if (EXPANDED_PRINCIPALS.length > 0 && !requiresAddonGlobal(worker)) {
      principals = EXPANDED_PRINCIPALS.concat(window);
      // We have to replace XHR constructor of the content document
      // with a custom cross origin one, automagically added by platform code:
      delete proto.XMLHttpRequest;
      wantGlobalProperties.push('XMLHttpRequest');
    }

    // Instantiate trusted code in another Sandbox in order to prevent content
    // script from messing with standard classes used by proxy and API code.
    let apiSandbox = sandbox(principals, { wantXrays: true, sameZoneAs: window });
    apiSandbox.console = console;

    // Create the sandbox and bind it to window in order for content scripts to
    // have access to all standard globals (window, document, ...)
    let content = sandbox(principals, {
      sandboxPrototype: proto,
      wantXrays: true,
      wantGlobalProperties: wantGlobalProperties,
      sameZoneAs: window,
      metadata: { SDKContentScript: true }
    });
    model.sandbox = content;
    
    // We have to ensure that window.top and window.parent are the exact same
    // object than window object, i.e. the sandbox global object. But not
    // always, in case of iframes, top and parent are another window object.
    let top = window.top === window ? content : content.top;
    let parent = window.parent === window ? content : content.parent;
    merge(content, {
      // We need 'this === window === top' to be true in toplevel scope:
      get window() content,
      get top() top,
      get parent() parent,
      // Use the Greasemonkey naming convention to provide access to the
      // unwrapped window object so the content script can access document
      // JavaScript values.
      // NOTE: this functionality is experimental and may change or go away
      // at any time!
      get unsafeWindow() window.wrappedJSObject
    });

    // Load trusted code that will inject content script API.
    // We need to expose JS objects defined in same principal in order to
    // avoid having any kind of wrapper.
    load(apiSandbox, CONTENT_WORKER_URL);

    // prepare a clean `self.options`
    let options = 'contentScriptOptions' in worker ?
      JSON.stringify(worker.contentScriptOptions) :
      undefined;

    // Then call `inject` method and communicate with this script
    // by trading two methods that allow to send events to the other side:
    //   - `onEvent` called by content script
    //   - `result.emitToContent` called by addon script
    // Bug 758203: We have to explicitely define `__exposedProps__` in order
    // to allow access to these chrome object attributes from this sandbox with
    // content priviledges
    // https://developer.mozilla.org/en/XPConnect_wrappers#Other_security_wrappers
    let onEvent = onContentEvent.bind(null, this);
    // `ContentWorker` is defined in CONTENT_WORKER_URL file
    let chromeAPI = createChromeAPI();
    let result = apiSandbox.ContentWorker.inject(content, chromeAPI, onEvent, options);

    // Merge `emitToContent` and `hasListenerFor` into our private
    // model of the WorkerSandbox so we can communicate with content
    // script
    merge(model, result);

    // Handle messages send by this script:
    setListeners(this);

    // Inject `addon` global into target document if document is trusted,
    // `addon` in document is equivalent to `self` in content script.
    if (requiresAddonGlobal(worker)) {
      Object.defineProperty(getUnsafeWindow(window), 'addon', {
          value: content.self
        }
      );
    }

    // Inject our `console` into target document if worker doesn't have a tab
    // (e.g Panel, PageWorker, Widget).
    // `worker.tab` can't be used because bug 804935.
    if (!getTabForContentWindow(window)) {
      let win = getUnsafeWindow(window);

      // export our chrome console to content window, using the same approach
      // of `ConsoleAPI`:
      // http://mxr.mozilla.org/mozilla-central/source/dom/base/ConsoleAPI.js#150
      //
      // and described here:
      // https://developer.mozilla.org/en-US/docs/Components.utils.createObjectIn
      let con = Cu.createObjectIn(win);

      let genPropDesc = function genPropDesc(fun) {
        return { enumerable: true, configurable: true, writable: true,
          value: console[fun] };
      }

      const properties = {
        log: genPropDesc('log'),
        info: genPropDesc('info'),
        warn: genPropDesc('warn'),
        error: genPropDesc('error'),
        debug: genPropDesc('debug'),
        trace: genPropDesc('trace'),
        dir: genPropDesc('dir'),
        group: genPropDesc('group'),
        groupCollapsed: genPropDesc('groupCollapsed'),
        groupEnd: genPropDesc('groupEnd'),
        time: genPropDesc('time'),
        timeEnd: genPropDesc('timeEnd'),
        profile: genPropDesc('profile'),
        profileEnd: genPropDesc('profileEnd'),
       __noSuchMethod__: { enumerable: true, configurable: true, writable: true,
                            value: function() {} }
      };

      Object.defineProperties(con, properties);
      Cu.makeObjectPropsNormal(con);

      win.console = con;
    };

    // The order of `contentScriptFile` and `contentScript` evaluation is
    // intentional, so programs can load libraries like jQuery from script URLs
    // and use them in scripts.
    let contentScriptFile = ('contentScriptFile' in worker) ? worker.contentScriptFile
          : null,
        contentScript = ('contentScript' in worker) ? worker.contentScript : null;

    if (contentScriptFile)
      importScripts.apply(null, [this].concat(contentScriptFile));
    if (contentScript) {
      evaluateIn(
        this,
        Array.isArray(contentScript) ? contentScript.join(';\n') : contentScript
      );
    }
  },
  destroy: function destroy() {
    this.emitSync('detach');
    let model = modelFor(this);
    model.sandbox = null
    model.worker = null;
  },

});

exports.WorkerSandbox = WorkerSandbox;

/**
 * Imports scripts to the sandbox by reading files under urls and
 * evaluating its source. If exception occurs during evaluation
 * `'error'` event is emitted on the worker.
 * This is actually an analog to the `importScript` method in web
 * workers but in our case it's not exposed even though content
 * scripts may be able to do it synchronously since IO operation
 * takes place in the UI process.
 */
function importScripts (workerSandbox, ...urls) {
  let { worker, sandbox } = modelFor(workerSandbox);
  for (let i in urls) {
    let contentScriptFile = urls[i];
    try {
      let uri = URL(contentScriptFile);
      if (uri.scheme === 'resource')
        load(sandbox, String(uri));
      else
        throw Error('Unsupported `contentScriptFile` url: ' + String(uri));
    }
    catch(e) {
      emit(worker, 'error', e);
    }
  }
}

function setListeners (workerSandbox) {
  let { worker } = modelFor(workerSandbox);
  // console.xxx calls
  workerSandbox.on('console', function consoleListener (kind, ...args) {
    console[kind].apply(console, args);
  });

  // self.postMessage calls
  workerSandbox.on('message', function postMessage(data) {
    // destroyed?
    if (worker)
      emit(worker, 'message', data);
  });

  // self.port.emit calls
  workerSandbox.on('event', function portEmit (...eventArgs) {
    // If not destroyed, emit event information to worker
    // `eventArgs` has the event name as first element,
    // and remaining elements are additional arguments to pass
    if (worker)
      emit.apply(null, [worker.port].concat(eventArgs));
  });

  // unwrap, recreate and propagate async Errors thrown from content-script
  workerSandbox.on('error', function onError({instanceOfError, value}) {
    if (worker) {
      let error = value;
      if (instanceOfError) {
        error = new Error(value.message, value.fileName, value.lineNumber);
        error.stack = value.stack;
        error.name = value.name;
      }
      emit(worker, 'error', error);
    }
  });
}

/**
 * Evaluates code in the sandbox.
 * @param {String} code
 *    JavaScript source to evaluate.
 * @param {String} [filename='javascript:' + code]
 *    Name of the file
 */
function evaluateIn (workerSandbox, code, filename) {
  let { worker, sandbox } = modelFor(workerSandbox);
  try {
    evaluate(sandbox, code, filename || 'javascript:' + code);
  }
  catch(e) {
    emit(worker, 'error', e);
  }
}

/**
 * Method called by the worker sandbox when it needs to send a message
 */
function onContentEvent (workerSandbox, args) {
  // As `emit`, we ensure having an asynchronous behavior
  async(function () {
    // We emit event to chrome/addon listeners
    emit.apply(null, [workerSandbox].concat(JSON.parse(args)));
  });
}


function modelFor (workerSandbox) {
  return sandboxes.get(workerSandbox);
}

/**
 * JSON.stringify is buggy with cross-sandbox values,
 * it may return '{}' on functions. Use a replacer to match them correctly.
 */
function replacer (k, v) {
  return typeof v === 'function' ? undefined : v;
}

function getUnsafeWindow (win) {
  return win.wrappedJSObject || win;
}

function emitToContent (workerSandbox, args) {
  return modelFor(workerSandbox).emitToContent(args);
}

function createChromeAPI () {
  return {
    timers: {
      setTimeout: timer.setTimeout,
      setInterval: timer.setInterval,
      clearTimeout: timer.clearTimeout,
      clearInterval: timer.clearInterval,
      __exposedProps__: {
        setTimeout: 'r',
        setInterval: 'r',
        clearTimeout: 'r',
        clearInterval: 'r'
      },
    },
    sandbox: {
      evaluate: evaluate,
      __exposedProps__: {
        evaluate: 'r'
      }
    },
    __exposedProps__: {
      timers: 'r',
      sandbox: 'r'
    }
  };
}
