/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Class } = require('../core/heritage');
const { EventTarget } = require('../event/target');
const { on, off, emit, setListeners } = require('../event/core');
const {
  attach, detach, destroy
} = require('./utils');
const { method } = require('../lang/functional');
const { Ci, Cu, Cc } = require('chrome');
const unload = require('../system/unload');
const events = require('../system/events');
const { getInnerId } = require("../window/utils");
const { WorkerSandbox } = require('./sandbox');
const { getTabForWindow } = require('../tabs/helpers');

// A weak map of workers to hold private attributes that
// should not be exposed
const workers = new WeakMap();

let modelFor = (worker) => workers.get(worker);

const ERR_DESTROYED =
  "Couldn't find the worker to receive this message. " +
  "The script may not be initialized yet, or may already have been unloaded.";

const ERR_FROZEN = "The page is currently hidden and can no longer be used " +
                   "until it is visible again.";


/**
 * Message-passing facility for communication between code running
 * in the content and add-on process.
 * @see https://addons.mozilla.org/en-US/developers/docs/sdk/latest/modules/sdk/content/worker.html
 */
const Worker = Class({
  implements: [EventTarget],
  initialize: function WorkerConstructor (options) {
    // Save model in weak map to not expose properties
    let model = createModel();
    workers.set(this, model);

    options = options || {};

    if ('contentScriptFile' in options)
      this.contentScriptFile = options.contentScriptFile;
    if ('contentScriptOptions' in options)
      this.contentScriptOptions = options.contentScriptOptions;
    if ('contentScript' in options)
      this.contentScript = options.contentScript;
    if ('injectInDocument' in options)
      this.injectInDocument = !!options.injectInDocument;

    setListeners(this, options);

    unload.ensure(this, "destroy");

    // Ensure that worker.port is initialized for contentWorker to be able
    // to send events during worker initialization.
    this.port = createPort(this);

    model.documentUnload = documentUnload.bind(this);
    model.pageShow = pageShow.bind(this);
    model.pageHide = pageHide.bind(this);

    if ('window' in options)
      attach(this, options.window);
  },

  /**
   * Sends a message to the worker's global scope. Method takes single
   * argument, which represents data to be sent to the worker. The data may
   * be any primitive type value or `JSON`. Call of this method asynchronously
   * emits `message` event with data value in the global scope of this
   * symbiont.
   *
   * `message` event listeners can be set either by calling
   * `self.on` with a first argument string `"message"` or by
   * implementing `onMessage` function in the global scope of this worker.
   * @param {Number|String|JSON} data
   */
  postMessage: function (...data) {
    let model = modelFor(this);
    let args = ['message'].concat(data);
    if (!model.inited) {
      model.earlyEvents.push(args);
      return;
    }
    processMessage.apply(null, [this].concat(args));
  },

  get url () {
    let model = modelFor(this);
    // model.window will be null after detach
    return model.window ? model.window.document.location.href : null;
  },

  get contentURL () {
    let model = modelFor(this);
    return model.window ? model.window.document.URL : null;
  },

  get tab () {
    let model = modelFor(this);
    // model.window will be null after detach
    if (model.window)
      return getTabForWindow(model.window);
    return null;
  },

  // Implemented to provide some of the previous features of exposing sandbox
  // so that Worker can be extended
  getSandbox: function () {
    return modelFor(this).contentWorker;
  },

  toString: function () { return '[object Worker]'; },
  attach: method(attach),
  detach: method(detach),
  destroy: method(destroy)
});
exports.Worker = Worker;

attach.define(Worker, function (worker, window) {
  let model = modelFor(worker);
  model.window = window;
  // Track document unload to destroy this worker.
  // We can't watch for unload event on page's window object as it
  // prevents bfcache from working:
  // https://developer.mozilla.org/En/Working_with_BFCache
  model.windowID = getInnerId(model.window);
  events.on("inner-window-destroyed", model.documentUnload);

  // Listen to pagehide event in order to freeze the content script
  // while the document is frozen in bfcache:
  model.window.addEventListener("pageshow", model.pageShow, true);
  model.window.addEventListener("pagehide", model.pageHide, true);

  // will set model.contentWorker pointing to the private API:
  model.contentWorker = WorkerSandbox(worker, model.window);

  // Mainly enable worker.port.emit to send event to the content worker
  model.inited = true;
  model.frozen = false;

  // Fire off `attach` event
  emit(worker, 'attach', window);

  // Process all events and messages that were fired before the
  // worker was initialized.
  model.earlyEvents.forEach(args => processMessage.apply(null, [worker].concat(args)));
});

/**
 * Remove all internal references to the attached document
 * Tells _port to unload itself and removes all the references from itself.
 */
detach.define(Worker, function (worker) {
  let model = modelFor(worker);
  // maybe unloaded before content side is created
  if (model.contentWorker)
    model.contentWorker.destroy();
  model.contentWorker = null;
  if (model.window) {
    model.window.removeEventListener("pageshow", model.pageShow, true);
    model.window.removeEventListener("pagehide", model.pageHide, true);
  }
  model.window = null;
  // This method may be called multiple times,
  // avoid dispatching `detach` event more than once
  if (model.windowID) {
    model.windowID = null;
    events.off("inner-window-destroyed", model.documentUnload);
    model.earlyEvents.length = 0;
    emit(worker, 'detach');
  }
  model.inited = false;
});

/**
 * Tells content worker to unload itself and
 * removes all the references from itself.
 */
destroy.define(Worker, function (worker) {
  detach(worker);
  modelFor(worker).inited = true;
  // Specifying no type or listener removes all listeners
  // from target
  off(worker);
});

/**
 * Events fired by workers
 */
function documentUnload ({ subject, data }) {
  let model = modelFor(this);
  let innerWinID = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
  if (innerWinID != model.windowID) return false;
  detach(this);
  return true;
}

function pageShow () {
  let model = modelFor(this);
  model.contentWorker.emitSync('pageshow');
  emit(this, 'pageshow');
  model.frozen = false;
}

function pageHide () {
  let model = modelFor(this);
  model.contentWorker.emitSync('pagehide');
  emit(this, 'pagehide');
  model.frozen = true;
}

/**
 * Fired from postMessage and emitEventToContent, or from the earlyMessage
 * queue when fired before the content is loaded. Sends arguments to
 * contentWorker if able
 */

function processMessage (worker, ...args) {
  let model = modelFor(worker) || {};
  if (!model.contentWorker)
    throw new Error(ERR_DESTROYED);
  if (model.frozen)
    throw new Error(ERR_FROZEN);

  model.contentWorker.emit.apply(null, args);
}

function createModel () {
  return {
    // List of messages fired before worker is initialized
    earlyEvents: [],
    // Is worker connected to the content worker sandbox ?
    inited: false,
    // Is worker being frozen? i.e related document is frozen in bfcache.
    // Content script should not be reachable if frozen.
    frozen: true,
    /**
     * Reference to the content side of the worker.
     * @type {WorkerGlobalScope}
     */
    contentWorker: null,
    /**
     * Reference to the window that is accessible from
     * the content scripts.
     * @type {Object}
     */
    window: null
  };
}

function createPort (worker) {
  let port = EventTarget();
  port.emit = emitEventToContent.bind(null, worker);
  return port;
}

/**
 * Emit a custom event to the content script,
 * i.e. emit this event on `self.port`
 */
function emitEventToContent (worker, ...eventArgs) {
  let model = modelFor(worker);
  let args = ['event'].concat(eventArgs);
  if (!model.inited) {
    model.earlyEvents.push(args);
    return;
  }
  processMessage.apply(null, [worker].concat(args));
}

