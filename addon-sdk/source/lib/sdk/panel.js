/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The panel module currently supports only Firefox and SeaMonkey.
// See: https://bugzilla.mozilla.org/show_bug.cgi?id=jetpack-panel-apps
module.metadata = {
  "stability": "stable",
  "engines": {
    "Firefox": "*",
    "SeaMonkey": "*"
  }
};

const { Ci } = require("chrome");
const { setTimeout } = require('./timers');
const { Class } = require("./core/heritage");
const { merge } = require("./util/object");
const { WorkerHost } = require("./content/utils");
const { Worker } = require("./deprecated/sync-worker");
const { Disposable } = require("./core/disposable");
const { WeakReference } = require('./core/reference');
const { contract: loaderContract } = require("./content/loader");
const { contract } = require("./util/contract");
const { on, off, emit, setListeners } = require("./event/core");
const { EventTarget } = require("./event/target");
const domPanel = require("./panel/utils");
const { getDocShell } = require('./frame/utils');
const { events } = require("./panel/events");
const systemEvents = require("./system/events");
const { filter, pipe, stripListeners } = require("./event/utils");
const { getNodeView, getActiveView } = require("./view/core");
const { isNil, isObject, isNumber } = require("./lang/type");
const { getAttachEventType } = require("./content/utils");
const { number, boolean, object } = require('./deprecated/api-utils');
const { Style } = require("./stylesheet/style");
const { attach, detach } = require("./content/mod");

var isRect = ({top, right, bottom, left}) => [top, right, bottom, left].
  some(value => isNumber(value) && !isNaN(value));

var isSDKObj = obj => obj instanceof Class;

var rectContract = contract({
  top: number,
  right: number,
  bottom: number,
  left: number
});

var position = {
  is: object,
  map: v => (isNil(v) || isSDKObj(v) || !isObject(v)) ? v : rectContract(v),
  ok: v => isNil(v) || isSDKObj(v) || (isObject(v) && isRect(v)),
  msg: 'The option "position" must be a SDK object registered as anchor; ' +
        'or an object with one or more of the following keys set to numeric ' +
        'values: top, right, bottom, left.'
}

var displayContract = contract({
  width: number,
  height: number,
  focus: boolean,
  position: position
});

var panelContract = contract(merge({
  // contentStyle* / contentScript* are sharing the same validation constraints,
  // so they can be mostly reused, except for the messages.
  contentStyle: merge(Object.create(loaderContract.rules.contentScript), {
    msg: 'The `contentStyle` option must be a string or an array of strings.'
  }),
  contentStyleFile: merge(Object.create(loaderContract.rules.contentScriptFile), {
    msg: 'The `contentStyleFile` option must be a local URL or an array of URLs'
  }),
  contextMenu: boolean,
  allow: {
    is: ['object', 'undefined', 'null'],
    map: function (allow) { return { script: !allow || allow.script !== false }}
  },
}, displayContract.rules, loaderContract.rules));

function Allow(panel) {
  return {
    get script() { return getDocShell(viewFor(panel).backgroundFrame).allowJavascript; },
    set script(value) { return setScriptState(panel, value); },
  };
}

function setScriptState(panel, value) {
  let view = viewFor(panel);
  getDocShell(view.backgroundFrame).allowJavascript = value;
  getDocShell(view.viewFrame).allowJavascript = value;
  view.setAttribute("sdkscriptenabled", "" + value);
}

function isDisposed(panel) !views.has(panel);

var panels = new WeakMap();
var models = new WeakMap();
var views = new WeakMap();
var workers = new WeakMap();
var styles = new WeakMap();

const viewFor = (panel) => views.get(panel);
const modelFor = (panel) => models.get(panel);
const panelFor = (view) => panels.get(view);
const workerFor = (panel) => workers.get(panel);
const styleFor = (panel) => styles.get(panel);

// Utility function takes `panel` instance and makes sure it will be
// automatically hidden as soon as other panel is shown.
var setupAutoHide = new function() {
  let refs = new WeakMap();

  return function setupAutoHide(panel) {
    // Create system event listener that reacts to any panel showing and
    // hides given `panel` if it's not the one being shown.
    function listener({subject}) {
      // It could be that listener is not GC-ed in the same cycle as
      // panel in such case we remove listener manually.
      let view = viewFor(panel);
      if (!view) systemEvents.off("popupshowing", listener);
      else if (subject !== view) panel.hide();
    }

    // system event listener is intentionally weak this way we'll allow GC
    // to claim panel if it's no longer referenced by an add-on code. This also
    // helps minimizing cleanup required on unload.
    systemEvents.on("popupshowing", listener);
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
    Disposable,
    WeakReference
  ],
  extends: WorkerHost(workerFor),
  setup: function setup(options) {
    let model = merge({
      defaultWidth: 320,
      defaultHeight: 240,
      focus: true,
      position: Object.freeze({}),
      contextMenu: false
    }, panelContract(options));
    model.ready = false;
    models.set(this, model);

    if (model.contentStyle || model.contentStyleFile) {
      styles.set(this, Style({
        uri: model.contentStyleFile,
        source: model.contentStyle
      }));
    }

    // Setup view
    let viewOptions = {allowJavascript: !model.allow || (model.allow.script !== false)};
    let view = domPanel.make(null, viewOptions);
    panels.set(view, this);
    views.set(this, view);

    // Load panel content.
    domPanel.setURL(view, model.contentURL);

    // Allow context menu
    domPanel.allowContextMenu(view, model.contextMenu);

    setupAutoHide(this);

    // Setup listeners.
    setListeners(this, options);
    let worker = new Worker(stripListeners(options));
    workers.set(this, worker);

    // pipe events from worker to a panel.
    pipe(worker, this);
  },
  dispose: function dispose() {
    this.hide();
    off(this);

    workerFor(this).destroy();
    detach(styleFor(this));

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

  /* Public API: Panel.contextMenu */
  get contextMenu() modelFor(this).contextMenu,
  set contextMenu(allow) {
    let model = modelFor(this);
    model.contextMenu = panelContract({ contextMenu: allow }).contextMenu;
    domPanel.allowContextMenu(viewFor(this), model.contextMenu);
  },

  get contentURL() modelFor(this).contentURL,
  set contentURL(value) {
    let model = modelFor(this);
    model.contentURL = panelContract({ contentURL: value }).contentURL;
    domPanel.setURL(viewFor(this), model.contentURL);
    // Detach worker so that messages send will be queued until it's
    // reatached once panel content is ready.
    workerFor(this).detach();
  },

  get allow() { return Allow(this); },
  set allow(value) {
    let allowJavascript = panelContract({ allow: value }).allow.script;
    return setScriptState(this, value);
  },

  /* Public API: Panel.isShowing */
  get isShowing() !isDisposed(this) && domPanel.isOpen(viewFor(this)),

  /* Public API: Panel.show */
  show: function show(options={}, anchor) {
    if (options instanceof Ci.nsIDOMElement) {
      [anchor, options] = [options, null];
    }

    if (anchor instanceof Ci.nsIDOMElement) {
      console.warn(
        "Passing a DOM node to Panel.show() method is an unsupported " +
        "feature that will be soon replaced. " +
        "See: https://bugzilla.mozilla.org/show_bug.cgi?id=878877"
      );
    }

    let model = modelFor(this);
    let view = viewFor(this);
    let anchorView = getNodeView(anchor || options.position || model.position);

    options = merge({
      position: model.position,
      width: model.width,
      height: model.height,
      defaultWidth: model.defaultWidth,
      defaultHeight: model.defaultHeight,
      focus: model.focus,
      contextMenu: model.contextMenu
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

// Note must be defined only after value to `Panel` is assigned.
getActiveView.define(Panel, viewFor);

// Filter panel events to only panels that are create by this module.
var panelEvents = filter(events, ({target}) => panelFor(target));

// Panel events emitted after panel has being shown.
var shows = filter(panelEvents, ({type}) => type === "popupshown");

// Panel events emitted after panel became hidden.
var hides = filter(panelEvents, ({type}) => type === "popuphidden");

// Panel events emitted after content inside panel is ready. For different
// panels ready may mean different state based on `contentScriptWhen` attribute.
// Weather given event represents readyness is detected by `getAttachEventType`
// helper function.
var ready = filter(panelEvents, ({type, target}) =>
  getAttachEventType(modelFor(panelFor(target))) === type);

// Panel event emitted when the contents of the panel has been loaded.
var readyToShow = filter(panelEvents, ({type}) => type === "DOMContentLoaded");

// Styles should be always added as soon as possible, and doesn't makes them
// depends on `contentScriptWhen`
var start = filter(panelEvents, ({type}) => type === "document-element-inserted");

// Forward panel show / hide events to panel's own event listeners.
on(shows, "data", ({target}) => {
  let panel = panelFor(target);
  if (modelFor(panel).ready)
    emit(panel, "show");
});

on(hides, "data", ({target}) => {
  let panel = panelFor(target);
  if (modelFor(panel).ready)
    emit(panel, "hide");
});

on(ready, "data", ({target}) => {
  let panel = panelFor(target);
  let window = domPanel.getContentDocument(target).defaultView;

  workerFor(panel).attach(window);
});

on(readyToShow, "data", ({target}) => {
  let panel = panelFor(target);

  if (!modelFor(panel).ready) {
    modelFor(panel).ready = true;

    if (viewFor(panel).state == "open")
      emit(panel, "show");
  }
});

on(start, "data", ({target}) => {
  let panel = panelFor(target);
  let window = domPanel.getContentDocument(target).defaultView;

  attach(styleFor(panel), window);
});
