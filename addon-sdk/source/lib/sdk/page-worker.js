/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const { Class } = require('./core/heritage');
const { ns } = require('./core/namespace');
const { pipe, stripListeners } = require('./event/utils');
const { connect, destroy, WorkerHost } = require('./content/utils');
const { Worker } = require('./content/worker');
const { Disposable } = require('./core/disposable');
const { EventTarget } = require('./event/target');
const { setListeners } = require('./event/core');
const { window } = require('./addon/window');
const { create: makeFrame, getDocShell } = require('./frame/utils');
const { contract } = require('./util/contract');
const { contract: loaderContract } = require('./content/loader');
const { Rules } = require('./util/rules');
const { merge } = require('./util/object');
const { uuid } = require('./util/uuid');
const { useRemoteProcesses, remoteRequire, frames } = require("./remote/parent");
remoteRequire("sdk/content/page-worker");

const workers = new WeakMap();
const pages = new Map();

const internal = ns();

let workerFor = (page) => workers.get(page);
let isDisposed = (page) => !pages.has(internal(page).id);

// The frame is used to ensure we have a remote process to load workers in
let remoteFrame = null;
let framePromise = null;
function getFrame() {
  if (framePromise)
    return framePromise;

  framePromise = new Promise(resolve => {
    let view = makeFrame(window.document, {
      namespaceURI: "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
      nodeName: "iframe",
      type: "content",
      remote: useRemoteProcesses,
      uri: "about:blank"
    });

    // Wait for the remote side to connect
    let listener = (frame) => {
      if (frame.frameElement != view)
        return;
      frames.off("attach", listener);
      remoteFrame = frame;
      resolve(frame);
    }
    frames.on("attach", listener);
  });
  return framePromise;
}

var pageContract = contract(merge({
  allow: {
    is: ['object', 'undefined', 'null'],
    map: function (allow) { return { script: !allow || allow.script !== false }}
  },
  onMessage: {
    is: ['function', 'undefined']
  },
  include: {
    is: ['string', 'array', 'regexp', 'undefined']
  },
  contentScriptWhen: {
    is: ['string', 'undefined'],
    map: (when) => when || "end"
  }
}, loaderContract.rules));

function enableScript (page) {
  getDocShell(viewFor(page)).allowJavascript = true;
}

function disableScript (page) {
  getDocShell(viewFor(page)).allowJavascript = false;
}

function Allow (page) {
  return {
    get script() {
      return internal(page).options.allow.script;
    },
    set script(value) {
      internal(page).options.allow.script = value;

      if (isDisposed(page))
        return;

      remoteFrame.port.emit("sdk/frame/set", internal(page).id, { allowScript: value });
    }
  };
}

function isValidURL(page, url) {
  return !page.rules || page.rules.matchesAny(url);
}

const Page = Class({
  implements: [
    EventTarget,
    Disposable
  ],
  extends: WorkerHost(workerFor),
  setup: function Page(options) {
    options = pageContract(options);
    // Sanitize the options
    if ("contentScriptOptions" in options)
      options.contentScriptOptions = JSON.stringify(options.contentScriptOptions);

    internal(this).id = uuid().toString();
    internal(this).options = options;

    for (let prop of ['contentScriptFile', 'contentScript', 'contentScriptWhen']) {
      this[prop] = options[prop];
    }

    pages.set(internal(this).id, this);

    // Set listeners on the {Page} object itself, not the underlying worker,
    // like `onMessage`, as it gets piped
    setListeners(this, options);
    let worker = new Worker(stripListeners(options));
    workers.set(this, worker);
    pipe(worker, this);

    if (options.include) {
      this.rules = Rules();
      this.rules.add.apply(this.rules, [].concat(options.include));
    }

    getFrame().then(frame => {
      if (isDisposed(this))
        return;

      frame.port.emit("sdk/frame/create", internal(this).id, stripListeners(options));
    });
  },
  get allow() { return Allow(this); },
  set allow(value) {
    if (isDisposed(this))
      return;
    this.allow.script = pageContract({ allow: value }).allow.script;
  },
  get contentURL() {
    return internal(this).options.contentURL;
  },
  set contentURL(value) {
    if (!isValidURL(this, value))
      return;
    internal(this).options.contentURL = value;
    if (isDisposed(this))
      return;

    remoteFrame.port.emit("sdk/frame/set", internal(this).id, { contentURL: value });
  },
  dispose: function () {
    if (isDisposed(this))
      return;
    pages.delete(internal(this).id);
    let worker = workerFor(this);
    if (worker)
      destroy(worker);
    remoteFrame.port.emit("sdk/frame/destroy", internal(this).id);

    // Destroy the remote frame if all the pages have been destroyed
    if (pages.size == 0) {
      framePromise = null;
      remoteFrame.frameElement.remove();
      remoteFrame = null;
    }
  },
  toString: function () { return '[object Page]' }
});

exports.Page = Page;

frames.port.on("sdk/frame/connect", (frame, id, params) => {
  let page = pages.get(id);
  if (!page)
    return;
  connect(workerFor(page), frame, params);
});
