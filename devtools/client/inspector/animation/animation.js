/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnimationsFront } = require("devtools/shared/fronts/animation");
const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./components/App"));
const { isAllTimingEffectEqual } = require("./utils/utils");
const { updateAnimations } = require("./actions/animations");
const { updateElementPickerEnabled } = require("./actions/element-picker");

class AnimationInspector {
  constructor(inspector) {
    this.inspector = inspector;

    this.toggleElementPicker = this.toggleElementPicker.bind(this);
    this.update = this.update.bind(this);
    this.onElementPickerStarted = this.onElementPickerStarted.bind(this);
    this.onElementPickerStopped = this.onElementPickerStopped.bind(this);

    this.init();
  }

  init() {
    const target = this.inspector.target;
    this.animationsFront = new AnimationsFront(target.client, target.form);

    const provider = createElement(Provider,
      {
        id: "newanimationinspector",
        key: "newanimationinspector",
        store: this.inspector.store
      },
      App(
        {
          toggleElementPicker: this.toggleElementPicker
        }
      )
    );
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.update);
    this.inspector.sidebar.on("newanimationinspector-selected", this.update);
    this.inspector.toolbox.on("picker-started", this.onElementPickerStarted);
    this.inspector.toolbox.on("picker-stopped", this.onElementPickerStopped);
  }

  destroy() {
    this.inspector.selection.off("new-node-front", this.update);
    this.inspector.sidebar.off("newanimationinspector-selected", this.update);
    this.inspector.toolbox.off("picker-started", this.onElementPickerStarted);
    this.inspector.toolbox.off("picker-stopped", this.onElementPickerStopped);

    this.inspector = null;
  }

  async update() {
    if (!this.inspector || !this.isPanelVisible()) {
      // AnimationInspector was destroyed already or the panel is hidden.
      return;
    }

    const done = this.inspector.updating("newanimationinspector");

    const selection = this.inspector.selection;
    const animations =
      selection.isConnected() && selection.isElementNode()
      ? await this.animationsFront.getAnimationPlayersForNode(selection.nodeFront)
      : [];

    if (!this.animations || !isAllTimingEffectEqual(animations, this.animations)) {
      this.inspector.store.dispatch(updateAnimations(animations));
      this.animations = animations;
    }

    done();
  }

  isPanelVisible() {
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "newanimationinspector";
  }

  toggleElementPicker() {
    this.inspector.toolbox.highlighterUtils.togglePicker();
  }

  onElementPickerStarted() {
    this.inspector.store.dispatch(updateElementPickerEnabled(true));
  }

  onElementPickerStopped() {
    this.inspector.store.dispatch(updateElementPickerEnabled(false));
  }
}

module.exports = AnimationInspector;
