/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const {
  Front,
  FrontClassWithSpec,
  preEvent,
  types
} = require("devtools/shared/protocol.js");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec
} = require("devtools/shared/specs/accessibility");

const events = require("devtools/shared/event-emitter");
const ACCESSIBLE_PROPERTIES = [
  "role",
  "name",
  "value",
  "description",
  "help",
  "keyboardShortcut",
  "childCount",
  "domNodeType"
];

const AccessibleFront = FrontClassWithSpec(accessibleSpec, {
  initialize(client, form) {
    Front.prototype.initialize.call(this, client, form);

    // Define getters for accesible properties that are received from the actor.
    // Note: we would like accessible properties to be iterable for a11y
    // clients.
    for (let key of ACCESSIBLE_PROPERTIES) {
      Object.defineProperty(this, key, {
        get() {
          return this._form[key];
        },
        enumerable: true
      });
    }
  },

  marshallPool() {
    return this.walker;
  },

  form(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }

    this.actorID = form.actor;
    this._form = form;
    DevToolsUtils.defineLazyGetter(this, "walker", () =>
      types.getType("accessiblewalker").read(this._form.walker, this));
  },

  /**
   * Get a dom node front from accessible actor's raw accessible object's
   * DONNode property.
   */
  getDOMNode(domWalker) {
    return domWalker.getNodeFromActor(this.actorID,
                                      ["rawAccessible", "DOMNode"]);
  },

  nameChange: preEvent("name-change", function (name, parent) {
    this._form.name = name;
    // Name change event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (this.walker) {
      events.emit(this.walker, "name-change", this, parent);
    }
  }),

  valueChange: preEvent("value-change", function (value) {
    this._form.value = value;
  }),

  descriptionChange: preEvent("description-change", function (description) {
    this._form.description = description;
  }),

  helpChange: preEvent("help-change", function (help) {
    this._form.help = help;
  }),

  shortcutChange: preEvent("shortcut-change", function (keyboardShortcut) {
    this._form.keyboardShortcut = keyboardShortcut;
  }),

  reorder: preEvent("reorder", function (childCount) {
    this._form.childCount = childCount;
    // Reorder event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (this.walker) {
      events.emit(this.walker, "reorder", this);
    }
  }),

  textChange: preEvent("text-change", function () {
    // Text event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (this.walker) {
      events.emit(this.walker, "text-change", this);
    }
  })
});

const AccessibleWalkerFront = FrontClassWithSpec(accessibleWalkerSpec, {
  accessibleDestroy: preEvent("accessible-destroy", function (accessible) {
    accessible.destroy();
  }),

  form(json) {
    this.actorID = json.actor;
  }
});

const AccessibilityFront = FrontClassWithSpec(accessibilitySpec, {
  initialize(client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.accessibilityActor;
    this.manage(this);
  }
});

exports.AccessibleFront = AccessibleFront;
exports.AccessibleWalkerFront = AccessibleWalkerFront;
exports.AccessibilityFront = AccessibilityFront;
