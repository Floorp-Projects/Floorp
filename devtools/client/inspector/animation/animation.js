/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnimationsFront } = require("devtools/shared/fronts/animation");
const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const EventEmitter = require("devtools/shared/event-emitter");

const App = createFactory(require("./components/App"));

const {
  updateAnimations,
  updateElementPickerEnabled,
  updateSelectedAnimation,
  updateSidebarSize
} = require("./actions/animations");
const { isAllAnimationEqual } = require("./utils/utils");

class AnimationInspector {
  constructor(inspector, win) {
    this.inspector = inspector;
    this.win = win;

    this.getAnimatedPropertyMap = this.getAnimatedPropertyMap.bind(this);
    this.getNodeFromActor = this.getNodeFromActor.bind(this);
    this.selectAnimation = this.selectAnimation.bind(this);
    this.simulateAnimation = this.simulateAnimation.bind(this);
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
      getAnimatedPropertyMap,
      getNodeFromActor,
      selectAnimation,
      simulateAnimation,
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
          getAnimatedPropertyMap,
          getNodeFromActor,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          selectAnimation,
          setSelectedNode,
          simulateAnimation,
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

    if (this.simulatedAnimation) {
      this.simulatedAnimation.cancel();
      this.simulatedAnimation = null;
    }

    if (this.simulatedElement) {
      this.simulatedElement.remove();
      this.simulatedElement = null;
    }

    this.inspector = null;
    this.win = null;
  }

  /**
   * Return a map of animated property from given animation actor.
   *
   * @param {Object} animation
   * @return {Map} A map of animated property
   *         key: {String} Animated property name
   *         value: {Array} Array of keyframe object
   *         Also, the keyframe object is consisted as following.
   *         {
   *           value: {String} style,
   *           offset: {Number} offset of keyframe,
   *           easing: {String} easing from this keyframe to next keyframe,
   *           distance: {Number} use as y coordinate in graph,
   *         }
   */
  async getAnimatedPropertyMap(animation) {
    let properties = [];

    try {
      properties = await animation.getProperties();
    } catch (e) {
      // Expected if we've already been destroyed in the meantime.
      console.error(e);
    }

    const animatedPropertyMap = new Map();

    for (const { name, values } of properties) {
      const keyframes = values.map(({ value, offset, easing, distance = 0 }) => {
        offset = parseFloat(offset.toFixed(3));
        return { value, offset, easing, distance };
      });

      animatedPropertyMap.set(name, keyframes);
    }

    return animatedPropertyMap;
  }

  getNodeFromActor(actorID) {
    return this.inspector.walker.getNodeFromActor(actorID, ["node"]);
  }

  isPanelVisible() {
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "newanimationinspector";
  }

  /**
   * Returns simulatable animation by given parameters.
   * The returned animation is implementing Animation interface of Web Animation API.
   * https://drafts.csswg.org/web-animations/#the-animation-interface
   *
   * @param {Array} keyframes
   *        e.g. [{ opacity: 0 }, { opacity: 1 }]
   * @param {Object} effectTiming
   *        e.g. { duration: 1000, fill: "both" }
   * @param {Boolean} isElementNeeded
   *        true:  create animation with an element.
   *               If want to know computed value of the element, turn on.
   *        false: create animation without an element,
   *               If need to know only timing progress.
   * @return {Animation}
   *         https://drafts.csswg.org/web-animations/#the-animation-interface
   */
  simulateAnimation(keyframes, effectTiming, isElementNeeded) {
    let targetEl = null;

    if (isElementNeeded) {
      if (!this.simulatedElement) {
        this.simulatedElement = this.win.document.createElement("div");
        this.win.document.documentElement.appendChild(this.simulatedElement);
      } else {
        // Reset styles.
        this.simulatedElement.style.cssText = "";
      }

      targetEl = this.simulatedElement;
    }

    if (!this.simulatedAnimation) {
      this.simulatedAnimation = new this.win.Animation();
    }

    this.simulatedAnimation.effect =
      new this.win.KeyframeEffect(targetEl, keyframes, effectTiming);

    return this.simulatedAnimation;
  }

  toggleElementPicker() {
    this.inspector.toolbox.highlighterUtils.togglePicker();
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

  selectAnimation(animation) {
    this.inspector.store.dispatch(updateSelectedAnimation(animation));
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
