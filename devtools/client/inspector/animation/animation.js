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
  updateDetailVisibility,
  updateElementPickerEnabled,
  updateSelectedAnimation,
  updateSidebarSize
} = require("./actions/animations");
const {
  isAllAnimationEqual,
  hasPlayingAnimation,
} = require("./utils/utils");

class AnimationInspector {
  constructor(inspector, win) {
    this.inspector = inspector;
    this.win = win;

    this.addAnimationsCurrentTimeListener =
      this.addAnimationsCurrentTimeListener.bind(this);
    this.getAnimatedPropertyMap = this.getAnimatedPropertyMap.bind(this);
    this.getComputedStyle = this.getComputedStyle.bind(this);
    this.getNodeFromActor = this.getNodeFromActor.bind(this);
    this.removeAnimationsCurrentTimeListener =
      this.removeAnimationsCurrentTimeListener.bind(this);
    this.rewindAnimationsCurrentTime = this.rewindAnimationsCurrentTime.bind(this);
    this.selectAnimation = this.selectAnimation.bind(this);
    this.setAnimationsCurrentTime = this.setAnimationsCurrentTime.bind(this);
    this.setAnimationsPlaybackRate = this.setAnimationsPlaybackRate.bind(this);
    this.setAnimationsPlayState = this.setAnimationsPlayState.bind(this);
    this.setDetailVisibility = this.setDetailVisibility.bind(this);
    this.simulateAnimation = this.simulateAnimation.bind(this);
    this.toggleElementPicker = this.toggleElementPicker.bind(this);
    this.update = this.update.bind(this);
    this.onAnimationsCurrentTimeUpdated = this.onAnimationsCurrentTimeUpdated.bind(this);
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
      addAnimationsCurrentTimeListener,
      emit: emitEventForTest,
      getAnimatedPropertyMap,
      getComputedStyle,
      getNodeFromActor,
      isAnimationsRunning,
      removeAnimationsCurrentTimeListener,
      rewindAnimationsCurrentTime,
      selectAnimation,
      setAnimationsCurrentTime,
      setAnimationsPlaybackRate,
      setAnimationsPlayState,
      setDetailVisibility,
      simulateAnimation,
      toggleElementPicker,
    } = this;

    const target = this.inspector.target;
    this.animationsFront = new AnimationsFront(target.client, target.form);

    this.animationsCurrentTimeListeners = [];
    this.isCurrentTimeSet = false;

    const provider = createElement(Provider,
      {
        id: "newanimationinspector",
        key: "newanimationinspector",
        store: this.inspector.store
      },
      App(
        {
          addAnimationsCurrentTimeListener,
          emitEventForTest,
          getAnimatedPropertyMap,
          getComputedStyle,
          getNodeFromActor,
          isAnimationsRunning,
          onHideBoxModelHighlighter,
          onShowBoxModelHighlighterForNode,
          removeAnimationsCurrentTimeListener,
          rewindAnimationsCurrentTime,
          selectAnimation,
          setAnimationsCurrentTime,
          setAnimationsPlaybackRate,
          setAnimationsPlayState,
          setDetailVisibility,
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

    this.stopAnimationsCurrentTimeTimer();

    this.inspector = null;
    this.win = null;
  }

  get state() {
    return this.inspector.store.getState().animations;
  }

  addAnimationsCurrentTimeListener(listener) {
    this.animationsCurrentTimeListeners.push(listener);
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

  /**
   * Return the computed style of the specified property after setting the given styles
   * to the simulated element.
   *
   * @param {String} property
   *        CSS property name (e.g. text-align).
   * @param {Object} styles
   *        Map of CSS property name and value.
   * @return {String}
   *         Computed style of property.
   */
  getComputedStyle(property, styles) {
    this.simulatedElement.style.cssText = "";

    for (let propertyName in styles) {
      this.simulatedElement.style.setProperty(propertyName, styles[propertyName]);
    }

    return this.win.getComputedStyle(this.simulatedElement).getPropertyValue(property);
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
   * This method should call when the current time is changed.
   * Then, dispatches the current time to listeners that are registered
   * by addAnimationsCurrentTimeListener.
   *
   * @param {Number} currentTime
   */
  onAnimationsCurrentTimeUpdated(currentTime) {
    for (const listener of this.animationsCurrentTimeListeners) {
      listener(currentTime);
    }
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

  removeAnimationsCurrentTimeListener(listener) {
    this.animationsCurrentTimeListeners =
      this.animationsCurrentTimeListeners.filter(l => l !== listener);
  }

  async rewindAnimationsCurrentTime() {
    await this.setAnimationsCurrentTime(0, true);
  }

  selectAnimation(animation) {
    this.inspector.store.dispatch(updateSelectedAnimation(animation));
  }

  async setAnimationsCurrentTime(currentTime, shouldRefresh) {
    this.stopAnimationsCurrentTimeTimer();
    this.onAnimationsCurrentTimeUpdated(currentTime);

    if (!shouldRefresh && this.isCurrentTimeSet) {
      return;
    }

    const animations = this.state.animations;
    this.isCurrentTimeSet = true;
    await this.animationsFront.setCurrentTimes(animations, currentTime, true);
    await this.updateAnimations(animations);
    this.isCurrentTimeSet = false;

    if (shouldRefresh) {
      this.updateState([...animations]);
    }
  }

  async setAnimationsPlaybackRate(playbackRate) {
    const animations = this.state.animations;
    await this.animationsFront.setPlaybackRates(animations, playbackRate);
    await this.updateAnimations(animations);
    await this.updateState([...animations]);
  }

  async setAnimationsPlayState(doPlay) {
    if (doPlay) {
      await this.animationsFront.playAll();
    } else {
      await this.animationsFront.pauseAll();
    }

    await this.updateAnimations(this.state.animations);
    await this.updateState([...this.state.animations]);
  }

  setDetailVisibility(isVisible) {
    this.inspector.store.dispatch(updateDetailVisibility(isVisible));
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

  stopAnimationsCurrentTimeTimer() {
    if (this.currentTimeTimer) {
      this.currentTimeTimer.destroy();
      this.currentTimeTimer = null;
    }
  }

  startAnimationsCurrentTimeTimer() {
    const currentTimeTimer = new CurrentTimeTimer(this);
    currentTimeTimer.start();
    this.currentTimeTimer = currentTimeTimer;
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
    const nextAnimations =
      selection.isConnected() && selection.isElementNode()
      ? await this.animationsFront.getAnimationPlayersForNode(selection.nodeFront)
      : [];
    const currentAnimations = this.state.animations;

    if (!currentAnimations || !isAllAnimationEqual(currentAnimations, nextAnimations)) {
      this.updateState(nextAnimations);
    }

    done();
  }

  async updateAnimations(animations) {
    const promises = animations.map(animation => {
      return animation.refreshState();
    });

    await Promise.all(promises);
  }

  updateState(animations) {
    this.stopAnimationsCurrentTimeTimer();

    // If number of displayed animations is one, we select the animation automatically.
    // But if selected animation is in given animations, ignores.
    const selectedAnimation = this.state.selectedAnimation;

    if (!selectedAnimation ||
        !animations.find(animation => animation.actorID === selectedAnimation.actorID)) {
      this.selectAnimation(animations.length === 1 ? animations[0] : null);
    }

    this.inspector.store.dispatch(updateAnimations(animations));

    if (hasPlayingAnimation(animations)) {
      this.startAnimationsCurrentTimeTimer();
    }
  }
}

class CurrentTimeTimer {
  constructor(animationInspector) {
    const timeScale = animationInspector.state.timeScale;
    this.baseCurrentTime = timeScale.documentCurrentTime - timeScale.minStartTime;
    this.startTime = animationInspector.win.performance.now();
    this.animationInspector = animationInspector;

    this.next = this.next.bind(this);
  }

  destroy() {
    this.stop();
    this.animationInspector = null;
  }

  next() {
    if (this.doStop) {
      return;
    }

    const { onAnimationsCurrentTimeUpdated, win } = this.animationInspector;
    const currentTime = this.baseCurrentTime + win.performance.now() - this.startTime;
    onAnimationsCurrentTimeUpdated(currentTime);
    win.requestAnimationFrame(this.next);
  }

  start() {
    this.next();
  }

  stop() {
    this.doStop = true;
  }
}

module.exports = AnimationInspector;
