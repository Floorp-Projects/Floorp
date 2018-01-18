/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnimationsFront } = require("devtools/shared/fronts/animation");
const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const EventEmitter = require("devtools/shared/event-emitter");

const App = createFactory(require("./components/App"));

const { updateAnimations } = require("./actions/animations");
const { updateElementPickerEnabled } = require("./actions/element-picker");
const { updateSidebarSize } = require("./actions/sidebar");
const { isAllAnimationEqual } = require("./utils/utils");

class AnimationInspector {
  constructor(inspector) {
    this.inspector = inspector;

    this.getNodeFromActor = this.getNodeFromActor.bind(this);
    this.toggleElementPicker = this.toggleElementPicker.bind(this);
    this.update = this.update.bind(this);
    this.onElementPickerStarted = this.onElementPickerStarted.bind(this);
    this.onElementPickerStopped = this.onElementPickerStopped.bind(this);
    this.onSidebarResized = this.onSidebarResized.bind(this);
    this.onSidebarSelect = this.onSidebarSelect.bind(this);

    EventEmitter.decorate(this);
    this.emit = this.emit.bind(this);

    this.init();
  }

  init() {
    const {
      setSelectedNode,
      onShowBoxModelHighlighterForNode,
    } = this.inspector.getCommonComponentProps();

    const {
      onHideBoxModelHighlighter,
    } = this.inspector.getPanel("boxmodel").getComponentProps();

    const {
      emit: emitEventForTest,
      getNodeFromActor,
      toggleElementPicker,
    } = this;

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
          emitEventForTest,
          getNodeFromActor,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          setSelectedNode,
          toggleElementPicker,
        }
      )
    );
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.update);
    this.inspector.sidebar.on("newanimationinspector-selected", this.onSidebarSelect);
    this.inspector.toolbox.on("inspector-sidebar-resized", this.onSidebarResized);
    this.inspector.toolbox.on("picker-started", this.onElementPickerStarted);
    this.inspector.toolbox.on("picker-stopped", this.onElementPickerStopped);
  }

  destroy() {
    this.inspector.selection.off("new-node-front", this.update);
    this.inspector.sidebar.off("newanimationinspector-selected", this.onSidebarSelect);
    this.inspector.toolbox.off("inspector-sidebar-resized", this.onSidebarResized);
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

    if (!this.animations || !isAllAnimationEqual(animations, this.animations)) {
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

  getNodeFromActor(actorID) {
    return this.inspector.walker.getNodeFromActor(actorID, ["node"]);
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

  onSidebarSelect() {
    this.update();
    this.onSidebarResized(null, this.inspector.getSidebarSize());
  }

  onSidebarResized(type, size) {
    if (!this.isPanelVisible()) {
      return;
    }

    this.inspector.store.dispatch(updateSidebarSize(size));
  }
}

module.exports = AnimationInspector;
