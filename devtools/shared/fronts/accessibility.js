/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  custom,
  Front,
  FrontClassWithSpec,
  preEvent
} = require("devtools/shared/protocol.js");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec
} = require("devtools/shared/specs/accessibility");
const events = require("devtools/shared/event-emitter");

const AccessibleFront = FrontClassWithSpec(accessibleSpec, {
  initialize(client, form) {
    Front.prototype.initialize.call(this, client, form);
  },

  marshallPool() {
    return this.parent();
  },

  get role() {
    return this._form.role;
  },

  get name() {
    return this._form.name;
  },

  get value() {
    return this._form.value;
  },

  get description() {
    return this._form.description;
  },

  get keyboardShortcut() {
    return this._form.keyboardShortcut;
  },

  get childCount() {
    return this._form.childCount;
  },

  get domNodeType() {
    return this._form.domNodeType;
  },

  get indexInParent() {
    return this._form.indexInParent;
  },

  get states() {
    return this._form.states;
  },

  get actions() {
    return this._form.actions;
  },

  get attributes() {
    return this._form.attributes;
  },

  form(form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }

    this.actorID = form.actor;
    this._form = form;
  },

  nameChange: preEvent("name-change", function(name, parent, walker) {
    this._form.name = name;
    // Name change event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "name-change", this, parent);
    }
  }),

  valueChange: preEvent("value-change", function(value) {
    this._form.value = value;
  }),

  descriptionChange: preEvent("description-change", function(description) {
    this._form.description = description;
  }),

  shortcutChange: preEvent("shortcut-change", function(keyboardShortcut) {
    this._form.keyboardShortcut = keyboardShortcut;
  }),

  reorder: preEvent("reorder", function(childCount, walker) {
    this._form.childCount = childCount;
    // Reorder event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "reorder", this);
    }
  }),

  textChange: preEvent("text-change", function(walker) {
    // Text event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "text-change", this);
    }
  }),

  indexInParentChange: preEvent("index-in-parent-change", function(indexInParent) {
    this._form.indexInParent = indexInParent;
  }),

  statesChange: preEvent("states-change", function(states) {
    this._form.states = states;
  }),

  actionsChange: preEvent("actions-change", function(actions) {
    this._form.actions = actions;
  }),

  attributesChange: preEvent("attributes-change", function(attributes) {
    this._form.attributes = attributes;
  })
});

const AccessibleWalkerFront = FrontClassWithSpec(accessibleWalkerSpec, {
  accessibleDestroy: preEvent("accessible-destroy", function(accessible) {
    accessible.destroy();
  }),

  form(json) {
    this.actorID = json.actor;
  },

  pick: custom(function(doFocus) {
    if (doFocus) {
      return this.pickAndFocus();
    }

    return this._pick();
  }, {
    impl: "_pick"
  })
});

const AccessibilityFront = FrontClassWithSpec(accessibilitySpec, {
  initialize(client, form) {
    Front.prototype.initialize.call(this, client, form);
    this.actorID = form.accessibilityActor;
    this.manage(this);
  },

  bootstrap: custom(function() {
    return this._bootstrap().then(state => {
      this.enabled = state.enabled;
      this.canBeEnabled = state.canBeEnabled;
      this.canBeDisabled = state.canBeDisabled;
    });
  }, {
    impl: "_bootstrap"
  }),

  init: preEvent("init", function() {
    this.enabled = true;
  }),

  shutdown: preEvent("shutdown", function() {
    this.enabled = false;
  }),

  canBeEnabled: preEvent("can-be-enabled-change", function(canBeEnabled) {
    this.canBeEnabled = canBeEnabled;
  }),

  canBeDisabled: preEvent("can-be-disabled-change", function(canBeDisabled) {
    this.canBeDisabled = canBeDisabled;
  })
});

exports.AccessibleFront = AccessibleFront;
exports.AccessibleWalkerFront = AccessibleWalkerFront;
exports.AccessibilityFront = AccessibilityFront;
