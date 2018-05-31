/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnimationsFront } = require("devtools/shared/fronts/animation");
const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const EventEmitter = require("devtools/shared/event-emitter");

const App = createFactory(require("./components/App"));
const CurrentTimeTimer = require("./current-time-timer");

const {
  updateAnimations,
  updateDetailVisibility,
  updateElementPickerEnabled,
  updateHighlightedNode,
  updateSelectedAnimation,
  updateSidebarSize
} = require("./actions/animations");
const {
  hasAnimationIterationCountInfinite,
  hasRunningAnimation,
} = require("./utils/utils");

class AnimationInspector {
  constructor(inspector, win) {
    this.inspector = inspector;
    this.win = win;

    this.addAnimationsCurrentTimeListener =
      this.addAnimationsCurrentTimeListener.bind(this);
    this.getAnimatedPropertyMap = this.getAnimatedPropertyMap.bind(this);
    this.getAnimationsCurrentTime = this.getAnimationsCurrentTime.bind(this);
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
    this.setHighlightedNode = this.setHighlightedNode.bind(this);
    this.setSelectedNode = this.setSelectedNode.bind(this);
    this.simulateAnimation = this.simulateAnimation.bind(this);
    this.simulateAnimationForKeyframesProgressBar =
      this.simulateAnimationForKeyframesProgressBar.bind(this);
    this.toggleElementPicker = this.toggleElementPicker.bind(this);
    this.update = this.update.bind(this);
    this.onAnimationStateChanged = this.onAnimationStateChanged.bind(this);
    this.onAnimationsCurrentTimeUpdated = this.onAnimationsCurrentTimeUpdated.bind(this);
    this.onAnimationsMutation = this.onAnimationsMutation.bind(this);
    this.onCurrentTimeTimerUpdated = this.onCurrentTimeTimerUpdated.bind(this);
    this.onElementPickerStarted = this.onElementPickerStarted.bind(this);
    this.onElementPickerStopped = this.onElementPickerStopped.bind(this);
    this.onSidebarResized = this.onSidebarResized.bind(this);
    this.onSidebarSelectionChanged = this.onSidebarSelectionChanged.bind(this);

    EventEmitter.decorate(this);
    this.emit = this.emit.bind(this);

    this.init();
  }

  init() {
    const {
      onShowBoxModelHighlighterForNode,
    } = this.inspector.getCommonComponentProps();

    const {
      onHideBoxModelHighlighter,
    } = this.inspector.getPanel("boxmodel").getComponentProps();

    const {
      addAnimationsCurrentTimeListener,
      emit: emitEventForTest,
      getAnimatedPropertyMap,
      getAnimationsCurrentTime,
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
      setHighlightedNode,
      setSelectedNode,
      simulateAnimation,
      simulateAnimationForKeyframesProgressBar,
      toggleElementPicker,
    } = this;

    const target = this.inspector.target;
    this.animationsFront = new AnimationsFront(target.client, target.form);
    this.animationsFront.setWalkerActor(this.inspector.walker);

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
          getAnimationsCurrentTime,
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
          setHighlightedNode,
          setSelectedNode,
          simulateAnimation,
          simulateAnimationForKeyframesProgressBar,
          toggleElementPicker,
        }
      )
    );
    this.provider = provider;

    this.inspector.sidebar.on("select", this.onSidebarSelectionChanged);
    this.inspector.toolbox.on("picker-started", this.onElementPickerStarted);
    this.inspector.toolbox.on("picker-stopped", this.onElementPickerStopped);
    this.inspector.toolbox.on("select", this.onSidebarSelectionChanged);
  }

  destroy() {
    this.setAnimationStateChangedListenerEnabled(false);
    this.inspector.selection.off("new-node-front", this.update);
    this.inspector.sidebar.off("select", this.onSidebarSelectionChanged);
    this.inspector.toolbox.off("inspector-sidebar-resized", this.onSidebarResized);
    this.inspector.toolbox.off("picker-started", this.onElementPickerStarted);
    this.inspector.toolbox.off("picker-stopped", this.onElementPickerStopped);
    this.inspector.toolbox.off("select", this.onSidebarSelectionChanged);

    this.animationsFront.off("mutations", this.onAnimationsMutation);

    if (this.simulatedAnimation) {
      this.simulatedAnimation.cancel();
      this.simulatedAnimation = null;
    }

    if (this.simulatedElement) {
      this.simulatedElement.remove();
      this.simulatedElement = null;
    }

    if (this.simulatedAnimationForKeyframesProgressBar) {
      this.simulatedAnimationForKeyframesProgressBar.cancel();
      this.simulatedAnimationForKeyframesProgressBar = null;
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
   * This function calls AnimationsFront.setCurrentTimes with considering the createdTime
   * which was introduced bug 1454392.
   *
   * @param {Number} currentTime
   */
  async doSetCurrentTimes(currentTime) {
    const { animations, timeScale } = this.state;

    // If currentTime is not defined in timeScale (which happens when connected
    // to server older than FF62), set currentTime as it is. See bug 1454392.
    currentTime = typeof timeScale.currentTime === "undefined"
                    ? currentTime : currentTime + timeScale.minStartTime;
    await this.animationsFront.setCurrentTimes(animations, currentTime, true,
                                               { relativeToCreatedTime: true });
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
    // getProperties might throw an error.
    const properties = await animation.getProperties();
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

  getAnimationsCurrentTime() {
    return this.currentTime;
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

    for (const propertyName in styles) {
      this.simulatedElement.style.setProperty(propertyName, styles[propertyName]);
    }

    return this.win.getComputedStyle(this.simulatedElement).getPropertyValue(property);
  }

  getNodeFromActor(actorID) {
    if (!this.inspector) {
      return Promise.reject("Animation inspector already destroyed");
    }

    return this.inspector.walker.getNodeFromActor(actorID, ["node"]);
  }

  isPanelVisible() {
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "newanimationinspector";
  }

  onAnimationStateChanged() {
    // Simply update the animations since the state has already been updated.
    this.updateState([...this.state.animations]);
  }

  /**
   * This method should call when the current time is changed.
   * Then, dispatches the current time to listeners that are registered
   * by addAnimationsCurrentTimeListener.
   *
   * @param {Number} currentTime
   */
  onAnimationsCurrentTimeUpdated(currentTime) {
    this.currentTime = currentTime;

    for (const listener of this.animationsCurrentTimeListeners) {
      listener(currentTime);
    }
  }

  /**
   * This method is called when the current time proceed by CurrentTimeTimer.
   *
   * @param {Number} currentTime
   * @param {Bool} shouldStop
   */
  onCurrentTimeTimerUpdated(currentTime, shouldStop) {
    if (shouldStop) {
      this.setAnimationsCurrentTime(currentTime, true);
    } else {
      this.onAnimationsCurrentTimeUpdated(currentTime);
    }
  }

  async onAnimationsMutation(changes) {
    let animations = [...this.state.animations];
    const addedAnimations = [];

    for (const {type, player: animation} of changes) {
      if (type === "added") {
        addedAnimations.push(animation);
        animation.on("changed", this.onAnimationStateChanged);
      } else if (type === "removed") {
        const index = animations.indexOf(animation);
        animations.splice(index, 1);
        animation.off("changed", this.onAnimationStateChanged);
      }
    }

    // Update existing other animations as well since the currentTime would be proceeded
    // sice the scrubber position is related the currentTime.
    // Also, don't update the state of removed animations since React components
    // may refer to the same instance still.
    await this.updateAnimations(animations);

    // Get rid of animations that were removed during async updateAnimations().
    animations = animations.filter(animation => !!animation.state.type);

    this.updateState(animations.concat(addedAnimations));
  }

  onElementPickerStarted() {
    this.inspector.store.dispatch(updateElementPickerEnabled(true));
  }

  onElementPickerStopped() {
    this.inspector.store.dispatch(updateElementPickerEnabled(false));
  }

  async onSidebarSelectionChanged() {
    const isPanelVisibled = this.isPanelVisible();

    if (this.wasPanelVisibled === isPanelVisibled) {
      // onSidebarSelectionChanged is called some times even same state
      // from sidebar and toolbar.
      return;
    }

    this.wasPanelVisibled = isPanelVisibled;

    if (this.isPanelVisible()) {
      await this.update();
      this.onSidebarResized(null, this.inspector.getSidebarSize());
      this.animationsFront.on("mutations", this.onAnimationsMutation);
      this.inspector.selection.on("new-node-front", this.update);
      this.inspector.toolbox.on("inspector-sidebar-resized", this.onSidebarResized);
    } else {
      this.stopAnimationsCurrentTimeTimer();
      this.animationsFront.off("mutations", this.onAnimationsMutation);
      this.inspector.selection.off("new-node-front", this.update);
      this.inspector.toolbox.off("inspector-sidebar-resized", this.onSidebarResized);
      this.setAnimationStateChangedListenerEnabled(false);
    }
  }

  onSidebarResized(size) {
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

  async setSelectedNode(nodeFront) {
    if (this.inspector.selection.nodeFront === nodeFront) {
      return;
    }

    await this.inspector.getCommonComponentProps()
              .setSelectedNode(nodeFront, { reason: "animation-panel" });
    await nodeFront.scrollIntoView();
  }

  async setAnimationsCurrentTime(currentTime, shouldRefresh) {
    this.stopAnimationsCurrentTimeTimer();
    this.onAnimationsCurrentTimeUpdated(currentTime);

    if (!shouldRefresh && this.isCurrentTimeSet) {
      return;
    }

    const { animations } = this.state;
    this.isCurrentTimeSet = true;

    try {
      await this.doSetCurrentTimes(currentTime);
      await this.updateAnimations(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    this.isCurrentTimeSet = false;

    if (shouldRefresh) {
      this.updateState([...animations]);
    }
  }

  async setAnimationsPlaybackRate(playbackRate) {
    const animations = this.state.animations;
    // "changed" event on each animation will fire respectively when the playback
    // rate changed. Since for each occurrence of event, change of UI is urged.
    // To avoid this, disable the listeners once in order to not capture the event.
    this.setAnimationStateChangedListenerEnabled(false);

    try {
      await this.animationsFront.setPlaybackRates(animations, playbackRate);
      await this.updateAnimations(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    } finally {
      this.setAnimationStateChangedListenerEnabled(true);
    }

    await this.updateState([...animations]);
  }

  async setAnimationsPlayState(doPlay) {
    if (typeof this.hasPausePlaySome === "undefined") {
      this.hasPausePlaySome =
        await this.inspector.target.actorHasMethod("animations", "pauseSome");
    }

    const { animations, timeScale } = this.state;

    try {
      if (doPlay && animations.every(animation =>
                      timeScale.getEndTime(animation) <= animation.state.currentTime)) {
        await this.doSetCurrentTimes(0);
      }

      // If the server does not support pauseSome/playSome function, (which happens
      // when connected to server older than FF62), use pauseAll/playAll instead.
      // See bug 1456857.
      if (this.hasPausePlaySome) {
        if (doPlay) {
          await this.animationsFront.playSome(animations);
        } else {
          await this.animationsFront.pauseSome(animations);
        }
      } else if (doPlay) {
        await this.animationsFront.playAll();
      } else {
        await this.animationsFront.pauseAll();
      }

      await this.updateAnimations(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    await this.updateState([...animations]);
  }

  /**
   * Enable/disable the animation state change listener.
   * If set true, observe "changed" event on current animations.
   * Otherwise, quit observing the "changed" event.
   *
   * @param {Bool} isEnabled
   */
  setAnimationStateChangedListenerEnabled(isEnabled) {
    if (isEnabled) {
      for (const animation of this.state.animations) {
        animation.on("changed", this.onAnimationStateChanged);
      }
    } else {
      for (const animation of this.state.animations) {
        animation.off("changed", this.onAnimationStateChanged);
      }
    }
  }

  setDetailVisibility(isVisible) {
    this.inspector.store.dispatch(updateDetailVisibility(isVisible));
  }

  /**
   * Highlight the given node with the box model highlighter.
   * If no node is provided, hide the box model highlighter.
   *
   * @param {NodeFront} nodeFront
   */
  async setHighlightedNode(nodeFront) {
    await this.inspector.highlighters.hideBoxModelHighlighter();

    if (nodeFront) {
      await this.inspector.highlighters.showBoxModelHighlighter(nodeFront);
    }

    this.inspector.store.dispatch(updateHighlightedNode(nodeFront));
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
    // Don't simulate animation if the animation inspector is already destroyed.
    if (!this.win) {
      return null;
    }

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

  /**
   * Returns a simulatable efect timing animation for the keyframes progress bar.
   * The returned animation is implementing Animation interface of Web Animation API.
   * https://drafts.csswg.org/web-animations/#the-animation-interface
   *
   * @param {Object} effectTiming
   *        e.g. { duration: 1000, fill: "both" }
   * @return {Animation}
   *         https://drafts.csswg.org/web-animations/#the-animation-interface
   */
  simulateAnimationForKeyframesProgressBar(effectTiming) {
    if (!this.simulatedAnimationForKeyframesProgressBar) {
      this.simulatedAnimationForKeyframesProgressBar = new this.win.Animation();
    }

    this.simulatedAnimationForKeyframesProgressBar.effect =
      new this.win.KeyframeEffect(null, null, effectTiming);

    return this.simulatedAnimationForKeyframesProgressBar;
  }

  stopAnimationsCurrentTimeTimer() {
    if (this.currentTimeTimer) {
      this.currentTimeTimer.destroy();
      this.currentTimeTimer = null;
    }
  }

  startAnimationsCurrentTimeTimer() {
    const timeScale = this.state.timeScale;
    const shouldStopAfterEndTime =
      !hasAnimationIterationCountInfinite(this.state.animations);

    const currentTimeTimer =
      new CurrentTimeTimer(timeScale, shouldStopAfterEndTime,
                           this.win, this.onCurrentTimeTimerUpdated);
    currentTimeTimer.start();
    this.currentTimeTimer = currentTimeTimer;
  }

  toggleElementPicker() {
    this.inspector.toolbox.highlighterUtils.togglePicker();
  }

  async update() {
    const done = this.inspector.updating("newanimationinspector");

    const selection = this.inspector.selection;
    const animations =
      selection.isConnected() && selection.isElementNode()
      ? await this.animationsFront.getAnimationPlayersForNode(selection.nodeFront)
      : [];
    this.updateState(animations);
    this.setAnimationStateChangedListenerEnabled(true);

    done();
  }

  updateAnimations(animations) {
    if (!animations.length) {
      return Promise.resolve();
    }

    return new Promise((resolve, reject) => {
      let count = 0;
      let error = null;

      for (const animation of animations) {
        animation.refreshState().catch(e => {
          error = e;
        }).finally(() => {
          count += 1;

          if (count === animations.length) {
            if (error) {
              reject(error);
            } else {
              resolve();
            }
          }
        });
      }
    });
  }

  updateState(animations) {
    // Animation inspector already destroyed
    if (!this.inspector) {
      return;
    }

    this.stopAnimationsCurrentTimeTimer();

    this.inspector.store.dispatch(updateAnimations(animations));

    if (hasRunningAnimation(animations)) {
      this.startAnimationsCurrentTimeTimer();
    } else {
      // Even no running animations, update the current time once
      // so as to show the state.
      this.onCurrentTimeTimerUpdated(this.state.timeScale.getCurrentTime());
    }
  }
}

module.exports = AnimationInspector;
