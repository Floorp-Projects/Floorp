/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental",
  "engines": {
    "Firefox": "> 28"
  }
};

const { Cu } = require("chrome");
const { CustomizableUI } = Cu.import('resource:///modules/CustomizableUI.jsm', {});
const { subscribe, send, Reactor, foldp, lift, merges } = require("../../event/utils");
const { InputPort } = require("../../input/system");
const { OutputPort } = require("../../output/system");
const { Interactive } = require("../../input/browser");
const { CustomizationInput } = require("../../input/customizable-ui");
const { pairs, map, isEmpty, object,
        each, keys, values } = require("../../util/sequence");
const { curry, flip } = require("../../lang/functional");
const { patch, diff } = require("diffpatcher/index");
const prefs = require("../../preferences/service");
const { getByOuterId } = require("../../window/utils");

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const PREF_ROOT = "extensions.sdk-toolbar-collapsed.";


// There are two output ports one for publishing changes that occured
// and the other for change requests. Later is synchronous and is only
// consumed here. Note: it needs to be synchronous to avoid race conditions
// when `collapsed` attribute changes are caused by user interaction and
// toolbar is destroyed between the ticks.
const output = new OutputPort({ id: "toolbar-changed" });
const syncoutput = new OutputPort({ id: "toolbar-change", sync: true });

// Merge disptached changes and recevied changes from models to keep state up to
// date.
const Toolbars = foldp(patch, {}, merges([new InputPort({ id: "toolbar-changed" }),
                                          new InputPort({ id: "toolbar-change" })]));
const State = lift((toolbars, windows, customizable) =>
  ({windows: windows, toolbars: toolbars, customizable: customizable}),
  Toolbars, Interactive, new CustomizationInput());

// Shared event handler that makes `event.target.parent` collapsed.
// Used as toolbar's close buttons click handler.
const collapseToolbar = event => {
  const toolbar = event.target.parentNode;
  toolbar.collapsed = true;
};

const parseAttribute = x =>
  x === "true" ? true :
  x === "false" ? false :
  x === "" ? null :
  x;

// Shared mutation observer that is used to observe `toolbar` node's
// attribute mutations. Mutations are aggregated in the `delta` hash
// and send to `ToolbarStateChanged` channel to let model know state
// has changed.
const attributesChanged = mutations => {
  const delta = mutations.reduce((changes, {attributeName, target}) => {
    const id = target.id;
    const field = attributeName === "toolbarname" ? "title" : attributeName;
    let change = changes[id] || (changes[id] = {});
    change[field] = parseAttribute(target.getAttribute(attributeName));
    return changes;
  }, {});

  // Calculate what are the updates from the current state and if there are
  // any send them.
  const updates = diff(reactor.value, patch(reactor.value, delta));

  if (!isEmpty(pairs(updates))) {
    // TODO: Consider sending sync to make sure that there won't be a new
    // update doing a delete in the meantime.
    send(syncoutput, updates);
  }
};


// Utility function creates `toolbar` with a "close" button and returns
// it back. In addition it set's up a listener and observer to communicate
// state changes.
const addView = curry((options, {document}) => {
  let view = document.createElementNS(XUL_NS, "toolbar");
  view.setAttribute("id", options.id);
  view.setAttribute("collapsed", options.collapsed);
  view.setAttribute("toolbarname", options.title);
  view.setAttribute("pack", "end");
  view.setAttribute("customizable", "false");
  view.setAttribute("style", "padding: 2px 0; max-height: 40px;");
  view.setAttribute("mode", "icons");
  view.setAttribute("iconsize", "small");
  view.setAttribute("context", "toolbar-context-menu");
  view.setAttribute("class", "chromeclass-toolbar");

  let label = document.createElementNS(XUL_NS, "label");
  label.setAttribute("value", options.title);
  label.setAttribute("collapsed", "true");
  view.appendChild(label);

  let closeButton = document.createElementNS(XUL_NS, "toolbarbutton");
  closeButton.setAttribute("id", "close-" + options.id);
  closeButton.setAttribute("class", "close-icon");
  closeButton.setAttribute("customizable", false);
  closeButton.addEventListener("command", collapseToolbar);

  view.appendChild(closeButton);

  // In order to have a close button not costumizable, aligned on the right,
  // leaving the customizable capabilities of Australis, we need to create
  // a toolbar inside a toolbar.
  // This is should be a temporary hack, we should have a proper XBL for toolbar
  // instead. See:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=982005
  let toolbar = document.createElementNS(XUL_NS, "toolbar");
  toolbar.setAttribute("id", "inner-" + options.id);
  toolbar.setAttribute("defaultset", options.items.join(","));
  toolbar.setAttribute("customizable", "true");
  toolbar.setAttribute("style", "-moz-appearance: none; overflow: hidden");
  toolbar.setAttribute("mode", "icons");
  toolbar.setAttribute("iconsize", "small");
  toolbar.setAttribute("context", "toolbar-context-menu");
  toolbar.setAttribute("flex", "1");

  view.insertBefore(toolbar, closeButton);

  const observer = new document.defaultView.MutationObserver(attributesChanged);
  observer.observe(view, { attributes: true,
                           attributeFilter: ["collapsed", "toolbarname"] });

  const toolbox = document.getElementById("navigator-toolbox");
  toolbox.appendChild(view);
});
const viewAdd = curry(flip(addView));

const removeView = curry((id, {document}) => {
  const view = document.getElementById(id);
  if (view) view.remove();
});

const updateView = curry((id, {title, collapsed, isCustomizing}, {document}) => {
  const view = document.getElementById(id);

  if (!view)
    return;

  if (title)
    view.setAttribute("toolbarname", title);

  if (collapsed !== void(0))
    view.setAttribute("collapsed", Boolean(collapsed));

  if (isCustomizing !== void(0)) {
    view.querySelector("label").collapsed = !isCustomizing;
    view.querySelector("toolbar").style.visibility = isCustomizing
                                                ? "hidden" : "visible";
  }
});

const viewUpdate = curry(flip(updateView));

// Utility function used to register toolbar into CustomizableUI.
const registerToolbar = state => {
  // If it's first additon register toolbar as customizableUI component.
  CustomizableUI.registerArea("inner-" + state.id, {
    type: CustomizableUI.TYPE_TOOLBAR,
    legacy: true,
    defaultPlacements: [...state.items]
  });
};
// Utility function used to unregister toolbar from the CustomizableUI.
const unregisterToolbar = CustomizableUI.unregisterArea;

const reactor = new Reactor({
  onStep: (present, past) => {
    const delta = diff(past, present);

    each(([id, update]) => {
      // If update is `null` toolbar is removed, in such case
      // we unregister toolbar and remove it from each window
      // it was added to.
      if (update === null) {
        unregisterToolbar("inner-" + id);
        each(removeView(id), values(past.windows));

        send(output, object([id, null]));
      }
      else if (past.toolbars[id]) {
        // If `collapsed` state for toolbar was updated, persist
        // it for a future sessions.
        if (update.collapsed !== void(0))
          prefs.set(PREF_ROOT + id, update.collapsed);

        // Reflect update in each window it was added to.
        each(updateView(id, update), values(past.windows));

        send(output, object([id, update]));
      }
      // Hack: Mutation observers are invoked async, which means that if
      // client does `hide(toolbar)` & then `toolbar.destroy()` by the
      // time we'll get update for `collapsed` toolbar will be removed.
      // For now we check if `update.id` is present which will be undefined
      // in such cases.
      else if (update.id) {
        // If it is a new toolbar we create initial state by overriding
        // `collapsed` filed with value persisted in previous sessions.
        const state = patch(update, {
          collapsed: prefs.get(PREF_ROOT + id, update.collapsed),
        });

        // Register toolbar and add it each window known in the past
        // (note that new windows if any will be handled in loop below).
        registerToolbar(state);
        each(addView(state), values(past.windows));

        send(output, object([state.id, state]));
      }
    }, pairs(delta.toolbars));

    // Add views to every window that was added.
    each(window => {
      if (window)
        each(viewAdd(window), values(past.toolbars));
    }, values(delta.windows));

    each(([id, isCustomizing]) => {
      each(viewUpdate(getByOuterId(id), {isCustomizing: !!isCustomizing}),
        keys(present.toolbars));

    }, pairs(delta.customizable))
  },
  onEnd: state => {
    each(id => {
      unregisterToolbar("inner-" + id);
      each(removeView(id), values(state.windows));
    }, keys(state.toolbars));
  }
});
reactor.run(State);
