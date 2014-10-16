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
const { requiresAddonGlobal } = require('./utils');
const { delay: async } = require('../lang/functional');
const { Ci, Cu, Cc } = require('chrome');
const timer = require('../timers');
const { URL } = require('../url');
const { sandbox, evaluate, load } = require('../loader/sandbox');
const { merge } = require('../util/object');
const { getTabForContentWindow } = require('../tabs/utils');
const { getInnerId } = require('../window/utils');
const { PlainTextConsole } = require('../console/plain-text');
const { data } = require('../self');
// WeakMap of sandboxes so we can access private values
const sandboxes = new WeakMap();

/* Trick the linker in order to ensure shipping these files in the XPI.
  require('./content-worker.js');
  Then, retrieve URL of these files in the XPI:
*/
let prefix = module.uri.split('sandbox.js')[0];
const CONTENT_WORKER_URL = prefix + 'content-worker.js';
const metadata = require('@loader/options').metadata;

// Fetch additional list of domains to authorize access to for each content
// script. It is stored in manifest `metadata` field which contains
// package.json data. This list is originaly defined by authors in
// `permissions` attribute of their package.json addon file.
const permissions = (metadata && metadata['permissions']) || {};
const EXPANDED_PRINCIPALS = permissions['cross-domain-content'] || [];

const waiveSecurityMembrane = !!permissions['unsafe-content-script'];

const nsIScriptSecurityManager = Ci.nsIScriptSecurityManager;
const secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].
  getService(Ci.nsIScriptSecurityManager);

const JS_VERSION = '1.8';

const WorkerSandbox = Class({
  implements: [ EventTarget ],

  /**
   * Emit a message to the worker content sandbox
   */
  emit: function emit(type, ...args) {
    // JSON.stringify is buggy with cross-sandbox values,
    // it may return "{}" on functions. Use a replacer to match them correctly.
    let replacer = (k, v) =>
      typeof(v) === "function"
        ? (type === "console" ? Function.toString.call(v) : void(0))
        : v;

    // Ensure having an asynchronous behavior
    async(() =>
      emitToContent(this, JSON.stringify([type, ...args], replacer))
    );
  },

  /**
   * Synchronous version of `emit`.
   * /!\ Should only be used when it is strictly mandatory /!\
   *     Doesn't ensure passing only JSON values.
   *     Mainly used by context-menu in order to avoid breaking it.
   */
  emitSync: function emitSync(...args) {
    // because the arguments could be also non JSONable values,
    // we need to ensure the array instance is created from
    // the content's sandbox
    return emitToContent(this, new modelFor(this).sandbox.Array(...args));
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

    // Use expanded principal for content-script if the content is a
    // regular web content for better isolation.
    // (This behavior can be turned off for now with the unsafe-content-script
    // flag to give addon developers time for making the necessary changes)
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
    let isSystemPrincipal = secMan.isSystemPrincipal(
      window.document.nodePrincipal);
    if (!isSystemPrincipal && !requiresAddonGlobal(worker)) {
      if (EXPANDED_PRINCIPALS.length > 0) {
        // We have to replace XHR constructor of the content document
        // with a custom cross origin one, automagically added by platform code:
        delete proto.XMLHttpRequest;
        wantGlobalProperties.push('XMLHttpRequest');
      }
      if (!waiveSecurityMembrane)
        principals = EXPANDED_PRINCIPALS.concat(window);
    }

    // Create the sandbox and bind it to window in order for content scripts to
    // have access to all standard globals (window, document, ...)
    let content = sandbox(principals, {
      sandboxPrototype: proto,
      wantXrays: !requiresAddonGlobal(worker),
      wantGlobalProperties: wantGlobalProperties,
      wantExportHelpers: true,
      sameZoneAs: window,
      metadata: {
        SDKContentScript: true,
        'inner-window-id': getInnerId(window)
      }
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
      get parent() parent
    });
    // Use the Greasemonkey naming convention to provide access to the
    // unwrapped window object so the content script can access document
    // JavaScript values.
    // NOTE: this functionality is experimental and may change or go away
    // at any time!
    //
    // Note that because waivers aren't propagated between origins, we
    // need the unsafeWindow getter to live in the sandbox.
    var unsafeWindowGetter =
      new content.Function('return window.wrappedJSObject || window;');
    Object.defineProperty(content, 'unsafeWindow', {get: unsafeWindowGetter});

    // Load trusted code that will inject content script API.
    let ContentWorker = load(content, CONTENT_WORKER_URL);

    // prepare a clean `self.options`
    let options = 'contentScriptOptions' in worker ?
      JSON.stringify(worker.contentScriptOptions) :
      undefined;

    // Then call `inject` method and communicate with this script
    // by trading two methods that allow to send events to the other side:
    //   - `onEvent` called by content script
    //   - `result.emitToContent` called by addon script
    let onEvent = onContentEvent.bind(null, this);
    let chromeAPI = createChromeAPI(ContentWorker);
    let result = Cu.waiveXrays(ContentWorker).inject(content, chromeAPI, onEvent, options);

    // Merge `emitToContent` into our private model of the
    // WorkerSandbox so we can communicate with content script
    model.emitToContent = result;

    let console = new PlainTextConsole(null, getInnerId(window));

    // Handle messages send by this script:
    setListeners(this, console);

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

      // export our chrome console to content window, as described here:
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
    let contentScriptFile = ('contentScriptFile' in worker)
          ? worker.contentScriptFile
          : null,
        contentScript = ('contentScript' in worker)
          ? worker.contentScript
          : null;

    if (contentScriptFile)
      importScripts.apply(null, [this].concat(contentScriptFile));
    if (contentScript) {
      evaluateIn(
        this,
        Array.isArray(contentScript) ? contentScript.join(';\n') : contentScript
      );
    }
  },
  destroy: function destroy(reason) {
    if (typeof reason != 'string')
      reason = '';
    this.emitSync('event', 'detach', reason);
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
    let contentScriptFile = data.url(urls[i]);

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

function setListeners (workerSandbox, console) {
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

function getUnsafeWindow (win) {
  return win.wrappedJSObject || win;
}

function emitToContent (workerSandbox, args) {
  return modelFor(workerSandbox).emitToContent(args);
}

function createChromeAPI (scope) {
  return Cu.cloneInto({
    timers: {
      setTimeout: timer.setTimeout.bind(timer),
      setInterval: timer.setInterval.bind(timer),
      clearTimeout: timer.clearTimeout.bind(timer),
      clearInterval: timer.clearInterval.bind(timer),
    },
    sandbox: {
      evaluate: evaluate,
    },
  }, scope, {cloneFunctions: true});
}
