/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { emit } = require('../event/core');
const { omit } = require('../util/object');
const { Class } = require('../core/heritage');
const { method } = require('../lang/functional');
const { getInnerId } = require('../window/utils');
const { EventTarget } = require('../event/target');
const { when, ensure } = require('../system/unload');
const { getTabForWindow } = require('../tabs/helpers');
const { getTabForContentWindow, getBrowserForTab } = require('../tabs/utils');
const { isPrivate } = require('../private-browsing/utils');
const { getFrameElement } = require('../window/utils');
const { attach, detach, destroy } = require('./utils');
const { on: observe } = require('../system/events');
const { uuid } = require('../util/uuid');
const { Ci, Cc } = require('chrome');

const ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"].
  getService(Ci.nsIMessageBroadcaster);

// null-out cycles in .modules to make @loader/options JSONable
const ADDON = omit(require('@loader/options'), ['modules', 'globals']);

const workers = new WeakMap();
let modelFor = (worker) => workers.get(worker);

const ERR_DESTROYED = "Couldn't find the worker to receive this message. " +
  "The script may not be initialized yet, or may already have been unloaded.";

const ERR_FROZEN = "The page is currently hidden and can no longer be used " +
                   "until it is visible again.";

// a handle for communication between content script and addon code
const Worker = Class({
  implements: [EventTarget],
  initialize(options = {}) {

    let model = {
      inited: false,
      earlyEvents: [],        // fired before worker was inited
      frozen: true,           // document is in BFcache, let it go
      options,
    };
    workers.set(this, model);

    ensure(this, 'destroy');
    this.on('detach', this.detach);
    EventTarget.prototype.initialize.call(this, options);

    this.receive = this.receive.bind(this);

    model.observe = ({ subject }) => {
      let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
      if (model.window && getInnerId(model.window) === id)
        this.detach();
    }

    observe('inner-window-destroyed', model.observe);

    this.port = EventTarget();
    this.port.emit = this.send.bind(this, 'event');
    this.postMessage = this.send.bind(this, 'message');

    if ('window' in options)
      attach(this, options.window);
  },
  // messages
  receive({ data: { id, args }}) {
    let model = modelFor(this);
    if (id !== model.id || !model.childWorker)
      return;
    if (args[0] === 'event')
      emit(this.port, ...args.slice(1))
    else
      emit(this, ...args);
  },
  send(...args) {
    let model = modelFor(this);
    if (!model.inited) {
      model.earlyEvents.push(args);
      return;
    }
    if (!model.childWorker && args[0] !== 'detach')
      throw new Error(ERR_DESTROYED);
    if (model.frozen && args[0] !== 'detach')
      throw new Error(ERR_FROZEN);
    try {
      model.manager.sendAsyncMessage('sdk/worker/message', { id: model.id, args });
    } catch (e) {
      //
    }
  },
  // properties
  get url() {
    let { window } = modelFor(this);
    return window && window.document.location.href;
  },
  get contentURL() {
    let { window } = modelFor(this);
    return window && window.document.URL;
  },
  get tab() {
    let { window } = modelFor(this);
    return window && getTabForWindow(window);
  },
  toString: () => '[object Worker]',
  // methods
  attach: method(attach),
  detach: method(detach),
  destroy: method(destroy),
})
exports.Worker = Worker;

attach.define(Worker, function(worker, window) {
  let model = modelFor(worker);

  model.window = window;
  model.options.window = getInnerId(window);
  model.options.currentReadyState = window.document.readyState;
  model.id = model.options.id = String(uuid());

  let tab = getTabForContentWindow(window);
  if (tab) {
    model.manager = getBrowserForTab(tab).messageManager;
  } else {
    model.manager = getFrameElement(window.top).frameLoader.messageManager;
  }

  model.manager.addMessageListener('sdk/worker/event', worker.receive);
  model.manager.addMessageListener('sdk/worker/attach', attach);

  model.manager.sendAsyncMessage('sdk/worker/create', {
    options: model.options,
    addon: ADDON
  });

  function attach({ data }) {
    if (data.id !== model.id)
      return;
    model.manager.removeMessageListener('sdk/worker/attach', attach);
    model.childWorker = true;

    worker.on('pageshow', () => model.frozen = false);
    worker.on('pagehide', () => model.frozen = true);

    model.inited = true;
    model.frozen = false;

    model.earlyEvents.forEach(args => worker.send(...args));
    emit(worker, 'attach', window);
  }
})

// unload and release the child worker, release window reference
detach.define(Worker, function(worker, reason) {
  let model = modelFor(worker);
  worker.send('detach', reason);
  if (!model.childWorker)
    return;

  model.childWorker = null;
  model.earlyEvents = [];
  model.window = null;
  emit(worker, 'detach');
  model.manager.removeMessageListener('sdk/worker/event', this.receive);
})

isPrivate.define(Worker, ({ tab }) => isPrivate(tab));

// unlod worker, release references
destroy.define(Worker, function(worker, reason) {
  detach(worker, reason);
  modelFor(worker).inited = true;
})

// unload Loaders used for creating WorkerChild instances in each process
when(() => ppmm.broadcastAsyncMessage('sdk/loader/unload', { data: ADDON }));
