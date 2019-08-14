/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol.js");
const {
  accessibleSpec,
  accessibleWalkerSpec,
  accessibilitySpec,
} = require("devtools/shared/specs/accessibility");
const events = require("devtools/shared/event-emitter");

class AccessibleFront extends FrontClassWithSpec(accessibleSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.before("audited", this.audited.bind(this));
    this.before("name-change", this.nameChange.bind(this));
    this.before("value-change", this.valueChange.bind(this));
    this.before("description-change", this.descriptionChange.bind(this));
    this.before("shortcut-change", this.shortcutChange.bind(this));
    this.before("reorder", this.reorder.bind(this));
    this.before("text-change", this.textChange.bind(this));
    this.before("index-in-parent-change", this.indexInParentChange.bind(this));
    this.before("states-change", this.statesChange.bind(this));
    this.before("actions-change", this.actionsChange.bind(this));
    this.before("attributes-change", this.attributesChange.bind(this));
  }

  marshallPool() {
    return this.parent();
  }

  get role() {
    return this._form.role;
  }

  get name() {
    return this._form.name;
  }

  get value() {
    return this._form.value;
  }

  get description() {
    return this._form.description;
  }

  get keyboardShortcut() {
    return this._form.keyboardShortcut;
  }

  get childCount() {
    return this._form.childCount;
  }

  get domNodeType() {
    return this._form.domNodeType;
  }

  get indexInParent() {
    return this._form.indexInParent;
  }

  get states() {
    return this._form.states;
  }

  get actions() {
    return this._form.actions;
  }

  get attributes() {
    return this._form.attributes;
  }

  get checks() {
    return this._form.checks;
  }

  form(form) {
    this.actorID = form.actor;
    this._form = this._form || {};
    Object.assign(this._form, form);
  }

  nameChange(name, parent, walker) {
    this._form.name = name;
    // Name change event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "name-change", this, parent);
    }
  }

  valueChange(value) {
    this._form.value = value;
  }

  descriptionChange(description) {
    this._form.description = description;
  }

  shortcutChange(keyboardShortcut) {
    this._form.keyboardShortcut = keyboardShortcut;
  }

  reorder(childCount, walker) {
    this._form.childCount = childCount;
    // Reorder event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "reorder", this);
    }
  }

  textChange(walker) {
    // Text event affects the tree rendering, we fire this event on
    // accessibility walker as the point of interaction for UI.
    if (walker) {
      events.emit(walker, "text-change", this);
    }
  }

  indexInParentChange(indexInParent) {
    this._form.indexInParent = indexInParent;
  }

  statesChange(states) {
    this._form.states = states;
  }

  actionsChange(actions) {
    this._form.actions = actions;
  }

  attributesChange(attributes) {
    this._form.attributes = attributes;
  }

  audited(checks) {
    this._form.checks = this._form.checks || {};
    Object.assign(this._form.checks, checks);
  }

  hydrate() {
    return super.hydrate().then(properties => {
      Object.assign(this._form, properties);
    });
  }
}

class AccessibleWalkerFront extends FrontClassWithSpec(accessibleWalkerSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this.before("accessible-destroy", this.accessibleDestroy.bind(this));
  }

  accessibleDestroy(accessible) {
    accessible.destroy();
  }

  form(json) {
    this.actorID = json.actor;
  }

  pick(doFocus) {
    if (doFocus) {
      return this.pickAndFocus();
    }

    return super.pick();
  }
}

class AccessibilityFront extends FrontClassWithSpec(accessibilitySpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.before("init", this.init.bind(this));
    this.before("shutdown", this.shutdown.bind(this));
    this.before("can-be-enabled-change", this.canBeEnabled.bind(this));
    this.before("can-be-disabled-change", this.canBeDisabled.bind(this));

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "accessibilityActor";
  }

  bootstrap() {
    return super.bootstrap().then(state => {
      this.enabled = state.enabled;
      this.canBeEnabled = state.canBeEnabled;
      this.canBeDisabled = state.canBeDisabled;
    });
  }

  init() {
    this.enabled = true;
  }

  shutdown() {
    this.enabled = false;
  }

  canBeEnabled(canBeEnabled) {
    this.canBeEnabled = canBeEnabled;
  }

  canBeDisabled(canBeDisabled) {
    this.canBeDisabled = canBeDisabled;
  }
}

exports.AccessibleFront = AccessibleFront;
registerFront(AccessibleFront);
exports.AccessibleWalkerFront = AccessibleWalkerFront;
registerFront(AccessibleWalkerFront);
exports.AccessibilityFront = AccessibilityFront;
registerFront(AccessibilityFront);
