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

const { Class } = require("../../core/heritage");
const { EventTarget } = require("../../event/target");
const { off, setListeners, emit } = require("../../event/core");
const { Reactor, foldp, merges, send } = require("../../event/utils");
const { Disposable } = require("../../core/disposable");
const { InputPort } = require("../../input/system");
const { OutputPort } = require("../../output/system");
const { identify } = require("../id");
const { pairs, object, map, each } = require("../../util/sequence");
const { patch, diff } = require("diffpatcher/index");
const { contract } = require("../../util/contract");
const { id: addonID } = require("../../self");

// Input state is accumulated from the input received form the toolbar
// view code & local output. Merging local output reflects local state
// changes without complete roundloop.
const input = foldp(patch, {}, new InputPort({ id: "toolbar-changed" }));
const output = new OutputPort({ id: "toolbar-change" });

// Takes toolbar title and normalizes is to an
// identifier, also prefixes with add-on id.
const titleToId = title =>
  ("toolbar-" + addonID + "-" + title).
    toLowerCase().
    replace(/\s/g, "-").
    replace(/[^A-Za-z0-9_\-]/g, "");

const validate = contract({
  title: {
    is: ["string"],
    ok: x => x.length > 0,
    msg: "The `option.title` string must be provided"
  },
  items: {
    is:["undefined", "object", "array"],
    msg: "The `options.items` must be iterable sequence of items"
  },
  hidden: {
    is: ["boolean", "undefined"],
    msg: "The `options.hidden` must be boolean"
  }
});

// Toolbars is a mapping between `toolbar.id` & `toolbar` instances,
// which is used to find intstance for dispatching events.
let toolbars = new Map();

const Toolbar = Class({
  extends: EventTarget,
  implements: [Disposable],
  initialize: function(params={}) {
    const options = validate(params);
    const id = titleToId(options.title);

    if (toolbars.has(id))
      throw Error("Toolbar with this id already exists: " + id);

    // Set of the items in the toolbar isn't mutable, as a matter of fact
    // it just defines desired set of items, actual set is under users
    // control. Conver test to an array and freeze to make sure users won't
    // try mess with it.
    const items = Object.freeze(options.items ? [...options.items] : []);

    const initial = {
      id: id,
      title: options.title,
      // By default toolbars are visible when add-on is installed, unless
      // add-on authors decides it should be hidden. From that point on
      // user is in control.
      collapsed: !!options.hidden,
      // In terms of state only identifiers of items matter.
      items: items.map(identify)
    };

    this.id = id;
    this.items = items;

    toolbars.set(id, this);
    setListeners(this, params);

    // Send initial state to the host so it can reflect it
    // into a user interface.
    send(output, object([id, initial]));
  },

  get title() {
    const state = reactor.value[this.id];
    return state && state.title;
  },
  get hidden() {
    const state = reactor.value[this.id];
    return state && state.collapsed;
  },

  destroy: function() {
    send(output, object([this.id, null]));
  },
  // `JSON.stringify` serializes objects based of the return
  // value of this method. For convinienc we provide this method
  // to serialize actual state data. Note: items will also be
  // serialized so they should probably implement `toJSON`.
  toJSON: function() {
    return {
      id: this.id,
      title: this.title,
      hidden: this.hidden,
      items: this.items
    };
  }
});
exports.Toolbar = Toolbar;
identify.define(Toolbar, toolbar => toolbar.id);

const dispose = toolbar => {
  toolbars.delete(toolbar.id);
  emit(toolbar, "detach");
  off(toolbar);
};

const reactor = new Reactor({
  onStep: (present, past) => {
    const delta = diff(past, present);

    each(([id, update]) => {
      const toolbar = toolbars.get(id);

      // Remove
      if (!update)
        dispose(toolbar);
      // Add
      else if (!past[id])
        emit(toolbar, "attach");
      // Update
      else
        emit(toolbar, update.collapsed ? "hide" : "show", toolbar);
    }, pairs(delta));
  }
});
reactor.run(input);
