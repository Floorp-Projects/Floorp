/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const { emit } = require('../event/core');
const { omit, merge } = require('../util/object');
const { Class } = require('../core/heritage');
const { method } = require('../lang/functional');
const { getInnerId } = require('../window/utils');
const { EventTarget } = require('../event/target');
const { isPrivate } = require('../private-browsing/utils');
const { getTabForBrowser, getTabForContentWindowNoShim, getBrowserForTab } = require('../tabs/utils');
const { attach, connect, detach, destroy, makeChildOptions } = require('./utils');
const { ensure } = require('../system/unload');
const { on: observe } = require('../system/events');
const { Ci, Cu } = require('chrome');
const { modelFor: tabFor } = require('sdk/model/core');
const { remoteRequire, processes, frames } = require('../remote/parent');
remoteRequire('sdk/content/worker-child');

const workers = new WeakMap();
var modelFor = (worker) => workers.get(worker);

const ERR_DESTROYED = "Couldn't find the worker to receive this message. " +
  "The script may not be initialized yet, or may already have been unloaded.";

// a handle for communication between content script and addon code
const Worker = Class({
  implements: [EventTarget],

  initialize(options = {}) {
    ensure(this, 'detach');

    let model = {
      attached: false,
      destroyed: false,
      earlyEvents: [],        // fired before worker was attached
      frozen: true,           // document is not yet active
      options,
    };
    workers.set(this, model);

    this.on('detach', this.detach);
    EventTarget.prototype.initialize.call(this, options);

    this.receive = this.receive.bind(this);

    this.port = EventTarget();
    this.port.emit = this.send.bind(this, 'event');
    this.postMessage = this.send.bind(this, 'message');

    if ('window' in options) {
      let window = options.window;
      delete options.window;
      attach(this, window);
    }
  },

  // messages
  receive(process, id, args) {
    let model = modelFor(this);
    if (id !== model.id || !model.attached)
      return;
    args = JSON.parse(args);
    if (model.destroyed && args[0] != 'detach')
      return;

    if (args[0] === 'event')
      emit(this.port, ...args.slice(1))
    else
      emit(this, ...args);
  },

  send(...args) {
    let model = modelFor(this);
    if (model.destroyed && args[0] !== 'detach')
      throw new Error(ERR_DESTROYED);

    if (!model.attached) {
      model.earlyEvents.push(args);
      return;
    }

    processes.port.emit('sdk/worker/message', model.id, JSON.stringify(args));
  },

  // properties
  get url() {
    let { url } = modelFor(this);
    return url;
  },

  get contentURL() {
    return this.url;
  },

  get tab() {
    require('sdk/tabs');
    let { frame } = modelFor(this);
    if (!frame)
      return null;
    let rawTab = getTabForBrowser(frame.frameElement);
    return rawTab && tabFor(rawTab);
  },

  toString: () => '[object Worker]',

  detach: method(detach),
  destroy: method(destroy),
})
exports.Worker = Worker;

attach.define(Worker, function(worker, window) {
  let model = modelFor(worker);
  if (model.attached)
    detach(worker);

  let childOptions = makeChildOptions(model.options);
  processes.port.emitCPOW('sdk/worker/create', [childOptions], { window });

  let listener = (frame, id, url) => {
    if (id != childOptions.id)
      return;
    frames.port.off('sdk/worker/connect', listener);
    connect(worker, frame, { id, url });
  };
  frames.port.on('sdk/worker/connect', listener);
});

connect.define(Worker, function(worker, frame, { id, url }) {
  let model = modelFor(worker);
  if (model.attached)
    detach(worker);

  model.id = id;
  model.frame = frame;
  model.url = url;

  // Messages from content -> chrome come through the process message manager
  // since that lives longer than the frame message manager
  processes.port.on('sdk/worker/event', worker.receive);

  model.attached = true;
  model.destroyed = false;
  model.frozen = false;

  model.earlyEvents.forEach(args => worker.send(...args));
  model.earlyEvents = [];
  emit(worker, 'attach');
});

// unload and release the child worker, release window reference
detach.define(Worker, function(worker) {
  let model = modelFor(worker);
  if (!model.attached)
    return;

  processes.port.off('sdk/worker/event', worker.receive);
  model.attached = false;
  model.destroyed = true;
  emit(worker, 'detach');
});

isPrivate.define(Worker, ({ tab }) => isPrivate(tab));

// Something in the parent side has destroyed the worker, tell the child to
// detach, the child will respond when it has detached
destroy.define(Worker, function(worker, reason) {
  let model = modelFor(worker);
  model.destroyed = true;
  if (!model.attached)
    return;

  worker.send('detach', reason);
});
