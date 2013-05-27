/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The panel module currently supports only Firefox.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=jetpack-panel-apps
module.metadata = {
  "stability": "stable",
  "engines": {
    "Firefox": "*"
  }
};

const { Ci } = require("chrome");
const { validateOptions: valid } = require('./deprecated/api-utils');
const { setTimeout } = require('./timers');
const { isPrivateBrowsingSupported } = require('./self');
const { isWindowPBSupported } = require('./private-browsing/utils');
const { Class } = require("./core/heritage");
const { merge } = require("./util/object");
const { WorkerHost, Worker, detach, attach,
        requiresAddonGlobal } = require("./worker/utils");
const { Disposable } = require("./core/disposable");
const { contract: loaderContract } = require("./content/loader");
const { contract } = require("./util/contract");
const { on, off, emit, setListeners } = require("./event/core");
const { EventTarget } = require("./event/target");
const domPanel = require("./panel/utils");
const { events } = require("./panel/events");
const systemEvents = require("./system/events");
const { filter, pipe } = require("./event/utils");
const { getNodeView, getActiveView } = require("./view/core");
const { isNil, isObject } = require("./lang/type");

function getAttachEventType(model) {
  let when = model.contentScriptWhen;
  return requiresAddonGlobal(model) ? "sdk-panel-content-changed" :
         when === "start" ? "sdk-panel-content-changed" :
         when === "end" ? "sdk-panel-document-loaded" :
         when === "ready" ? "sdk-panel-content-loaded" :
         null;
}


let number = { is: ['number', 'undefined', 'null'] };
let boolean = { is: ['boolean', 'undefined', 'null'] };

let rectContract = contract({
  top: number,
  right: number,
  bottom: number,
  left: number
});

let rect = {
  is: ['object', 'undefined', 'null'],
  map: function(v) isNil(v) || !isObject(v) ? v : rectContract(v)
}

let displayContract = contract({
  width: number,
  height: number,
  focus: boolean,
  position: rect
});

let panelContract = contract(merge({}, displayContract.rules, loaderContract.rules));


function isDisposed(panel) !views.has(panel);

let panels = new WeakMap();
let models = new WeakMap();
let views = new WeakMap();
let workers = new WeakMap();

function viewFor(panel) views.get(panel)
function modelFor(panel) models.get(panel)
function panelFor(view) panels.get(view)
function workerFor(panel) workers.get(panel)

getActiveView.define(Panel, viewFor);

// Utility function takes `panel` instance and makes sure it will be
// automatically hidden as soon as other panel is shown.
let setupAutoHide = new function() {
  let refs = new WeakMap();

  return function setupAutoHide(panel) {
    // Create system event listener that reacts to any panel showing and
    // hides given `panel` if it's not the one being shown.
    function listener({subject}) {
      // It could be that listener is not GC-ed in the same cycle as
      // panel in such case we remove listener manually.
      let view = viewFor(panel);
      if (!view) systemEvents.off("sdk-panel-show", listener);
      else if (subject !== view) panel.hide();
    }

    // system event listener is intentionally weak this way we'll allow GC
    // to claim panel if it's no longer referenced by an add-on code. This also
    // helps minimizing cleanup required on unload.
    systemEvents.on("sdk-panel-show", listener);
    // To make sure listener is not claimed by GC earlier than necessary we
    // associate it with `panel` it's associated with. This way it won't be
    // GC-ed earlier than `panel` itself.
    refs.set(panel, listener);
  }
}

const Panel = Class({
  implements: [
    // Generate accessors for the validated properties that update model on
    // set and return values from model on get.
    panelContract.properties(modelFor),
    EventTarget,
    Disposable
  ],
  extends: WorkerHost(workerFor),
  setup: function setup(options) {
    let model = merge({
      defaultWidth: 320,
      defaultHeight: 240,
      focus: true,
      position: Object.freeze({}),
    }, panelContract(options));
    models.set(this, model);

    // Setup listeners.
    setListeners(this, options);

    // Setup view
    let view = domPanel.make();
    panels.set(view, this);
    views.set(this, view);

    // Load panel content.
    domPanel.setURL(view, model.contentURL);

    setupAutoHide(this);

    let worker = new Worker(options);
    workers.set(this, worker);

    // pipe events from worker to a panel.
    pipe(worker, this);
  },
  dispose: function dispose() {
    this.hide();
    off(this);

    detach(workerFor(this));

    domPanel.dispose(viewFor(this));

    // Release circular reference between view and panel instance. This
    // way view will be GC-ed. And panel as well once all the other refs
    // will be removed from it.
    views.delete(this);
  },
  /* Public API: Panel.width */
  get width() modelFor(this).width,
  set width(value) this.resize(value, this.height),
  /* Public API: Panel.height */
  get height() modelFor(this).height,
  set height(value) this.resize(this.width, value),

  /* Public API: Panel.focus */
  get focus() modelFor(this).focus,

  /* Public API: Panel.position */
  get position() modelFor(this).position,

  get contentURL() modelFor(this).contentURL,
  set contentURL(value) {
    let model = modelFor(this);
    model.contentURL = panelContract({ contentURL: value }).contentURL;
    domPanel.setURL(viewFor(this), model.contentURL);
  },

  /* Public API: Panel.isShowing */
  get isShowing() !isDisposed(this) && domPanel.isOpen(viewFor(this)),

  /* Public API: Panel.show */
  show: function show(options, anchor) {
    let model = modelFor(this);
    let view = viewFor(this);
    let anchorView = getNodeView(anchor);

    options = merge({
      position: model.position,
      width: model.width,
      height: model.height,
      defaultWidth: model.defaultWidth,
      defaultHeight: model.defaultHeight,
      focus: model.focus
    }, displayContract(options));

    if (!isDisposed(this))
      domPanel.show(view, options, anchorView);

    return this;
  },

  /* Public API: Panel.hide */
  hide: function hide() {
    // Quit immediately if panel is disposed or there is no state change.
    domPanel.close(viewFor(this));

    return this;
  },

  /* Public API: Panel.resize */
  resize: function resize(width, height) {
    let model = modelFor(this);
    let view = viewFor(this);
    let change = panelContract({
      width: width || model.width || model.defaultWidth,
      height: height || model.height || model.defaultHeight
    });

    model.width = change.width
    model.height = change.height

    domPanel.resize(view, model.width, model.height);

    return this;
  }
});
exports.Panel = Panel;

// Filter panel events to only panels that are create by this module.
let panelEvents = filter(events, function({target}) panelFor(target));

// Panel events emitted after panel has being shown.
let shows = filter(panelEvents, function({type}) type === "sdk-panel-shown");

// Panel events emitted after panel became hidden.
let hides = filter(panelEvents, function({type}) type === "sdk-panel-hidden");

// Panel events emitted after content inside panel is ready. For different
// panels ready may mean different state based on `contentScriptWhen` attribute.
// Weather given event represents readyness is detected by `getAttachEventType`
// helper function.
let ready = filter(panelEvents, function({type, target})
  getAttachEventType(modelFor(panelFor(target))) === type);

// Panel events emitted after content document in the panel has changed.
let change = filter(panelEvents, function({type})
  type === "sdk-panel-content-changed");

// Forward panel show / hide events to panel's own event listeners.
on(shows, "data", function({target}) emit(panelFor(target), "show"));
on(hides, "data", function({target}) emit(panelFor(target), "hide"));

on(ready, "data", function({target}) {
  let worker = workerFor(panelFor(target));
  attach(worker, domPanel.getContentDocument(target).defaultView);
});
