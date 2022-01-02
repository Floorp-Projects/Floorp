/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const EventEmitter = require("devtools/shared/event-emitter");

const App = createFactory(
  require("devtools/client/inspector/animation/components/App")
);
const CurrentTimeTimer = require("devtools/client/inspector/animation/current-time-timer");

const animationsReducer = require("devtools/client/inspector/animation/reducers/animations");
const {
  updateAnimations,
  updateDetailVisibility,
  updateElementPickerEnabled,
  updateHighlightedNode,
  updatePlaybackRates,
  updateSelectedAnimation,
  updateSidebarSize,
} = require("devtools/client/inspector/animation/actions/animations");
const {
  hasAnimationIterationCountInfinite,
  hasRunningAnimation,
} = require("devtools/client/inspector/animation/utils/utils");

class AnimationInspector {
  constructor(inspector, win) {
    this.inspector = inspector;
    this.win = win;

    this.inspector.store.injectReducer("animations", animationsReducer);

    this.addAnimationsCurrentTimeListener = this.addAnimationsCurrentTimeListener.bind(
      this
    );
    this.getAnimatedPropertyMap = this.getAnimatedPropertyMap.bind(this);
    this.getAnimationsCurrentTime = this.getAnimationsCurrentTime.bind(this);
    this.getComputedStyle = this.getComputedStyle.bind(this);
    this.getNodeFromActor = this.getNodeFromActor.bind(this);
    this.removeAnimationsCurrentTimeListener = this.removeAnimationsCurrentTimeListener.bind(
      this
    );
    this.rewindAnimationsCurrentTime = this.rewindAnimationsCurrentTime.bind(
      this
    );
    this.selectAnimation = this.selectAnimation.bind(this);
    this.setAnimationsCurrentTime = this.setAnimationsCurrentTime.bind(this);
    this.setAnimationsPlaybackRate = this.setAnimationsPlaybackRate.bind(this);
    this.setAnimationsPlayState = this.setAnimationsPlayState.bind(this);
    this.setDetailVisibility = this.setDetailVisibility.bind(this);
    this.setHighlightedNode = this.setHighlightedNode.bind(this);
    this.setSelectedNode = this.setSelectedNode.bind(this);
    this.simulateAnimation = this.simulateAnimation.bind(this);
    this.simulateAnimationForKeyframesProgressBar = this.simulateAnimationForKeyframesProgressBar.bind(
      this
    );
    this.toggleElementPicker = this.toggleElementPicker.bind(this);
    this.update = this.update.bind(this);
    this.onAnimationStateChanged = this.onAnimationStateChanged.bind(this);
    this.onAnimationsCurrentTimeUpdated = this.onAnimationsCurrentTimeUpdated.bind(
      this
    );
    this.onAnimationsMutation = this.onAnimationsMutation.bind(this);
    this.onCurrentTimeTimerUpdated = this.onCurrentTimeTimerUpdated.bind(this);
    this.onElementPickerStarted = this.onElementPickerStarted.bind(this);
    this.onElementPickerStopped = this.onElementPickerStopped.bind(this);
    this.onNavigate = this.onNavigate.bind(this);
    this.onSidebarResized = this.onSidebarResized.bind(this);
    this.onSidebarSelectionChanged = this.onSidebarSelectionChanged.bind(this);
    this.onTargetAvailable = this.onTargetAvailable.bind(this);

    EventEmitter.decorate(this);
    this.emitForTests = this.emitForTests.bind(this);

    this.initComponents();
    this.initListeners();
  }

  initComponents() {
    const {
      addAnimationsCurrentTimeListener,
      emitForTests: emitEventForTest,
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

    const direction = this.win.document.dir;

    this.animationsCurrentTimeListeners = [];
    this.isCurrentTimeSet = false;

    const provider = createElement(
      Provider,
      {
        id: "animationinspector",
        key: "animationinspector",
        store: this.inspector.store,
      },
      App({
        addAnimationsCurrentTimeListener,
        direction,
        emitEventForTest,
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
      })
    );
    this.provider = provider;
  }

  async initListeners() {
    await this.inspector.commands.targetCommand.watchTargets({
      types: [this.inspector.commands.targetCommand.TYPES.FRAME],
      onAvailable: this.onTargetAvailable,
    });

    this.inspector.on("new-root", this.onNavigate);
    this.inspector.selection.on("new-node-front", this.update);
    this.inspector.sidebar.on("select", this.onSidebarSelectionChanged);
    this.inspector.toolbox.on("select", this.onSidebarSelectionChanged);
    this.inspector.toolbox.on(
      "inspector-sidebar-resized",
      this.onSidebarResized
    );
    this.inspector.toolbox.nodePicker.on(
      "picker-started",
      this.onElementPickerStarted
    );
    this.inspector.toolbox.nodePicker.on(
      "picker-stopped",
      this.onElementPickerStopped
    );
  }

  destroy() {
    this.setAnimationStateChangedListenerEnabled(false);
    this.inspector.off("new-root", this.onNavigate);
    this.inspector.selection.off("new-node-front", this.update);
    this.inspector.sidebar.off("select", this.onSidebarSelectionChanged);
    this.inspector.toolbox.off(
      "inspector-sidebar-resized",
      this.onSidebarResized
    );
    this.inspector.toolbox.nodePicker.off(
      "picker-started",
      this.onElementPickerStarted
    );
    this.inspector.toolbox.nodePicker.off(
      "picker-stopped",
      this.onElementPickerStopped
    );
    this.inspector.toolbox.off("select", this.onSidebarSelectionChanged);

    if (this.animationsFront) {
      this.animationsFront.off("mutations", this.onAnimationsMutation);
    }

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
   * This function calls AnimationsFront.setCurrentTimes with considering the createdTime.
   *
   * @param {Number} currentTime
   */
  async doSetCurrentTimes(currentTime) {
    const { animations, timeScale } = this.state;
    currentTime = currentTime + timeScale.minStartTime;
    await this.animationsFront.setCurrentTimes(animations, currentTime, true, {
      relativeToCreatedTime: true,
    });
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
  getAnimatedPropertyMap(animation) {
    const properties = animation.state.properties;
    const animatedPropertyMap = new Map();

    for (const { name, values } of properties) {
      const keyframes = values.map(
        ({ value, offset, easing, distance = 0 }) => {
          offset = parseFloat(offset.toFixed(3));
          return { value, offset, easing, distance };
        }
      );

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
      this.simulatedElement.style.setProperty(
        propertyName,
        styles[propertyName]
      );
    }

    return this.win
      .getComputedStyle(this.simulatedElement)
      .getPropertyValue(property);
  }

  getNodeFromActor(actorID) {
    if (!this.inspector) {
      return Promise.reject("Animation inspector already destroyed");
    }

    return this.inspector.walker.getNodeFromActor(actorID, ["node"]);
  }

  isPanelVisible() {
    return (
      this.inspector &&
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() === "animationinspector"
    );
  }

  onAnimationStateChanged() {
    // Simply update the animations since the state has already been updated.
    this.fireUpdateAction([...this.state.animations]);
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

    for (const { type, player: animation } of changes) {
      if (type === "added") {
        if (!animation.state.type) {
          // This animation was added but removed immediately.
          continue;
        }

        addedAnimations.push(animation);
        animation.on("changed", this.onAnimationStateChanged);
      } else if (type === "removed") {
        const index = animations.indexOf(animation);

        if (index < 0) {
          // This animation was added but removed immediately.
          continue;
        }

        animations.splice(index, 1);
        animation.off("changed", this.onAnimationStateChanged);
      }
    }

    // Update existing other animations as well since the currentTime would be proceeded
    // sice the scrubber position is related the currentTime.
    // Also, don't update the state of removed animations since React components
    // may refer to the same instance still.
    try {
      animations = await this.refreshAnimationsState(animations);
    } catch (_) {
      console.error(`Updating Animations failed`);
      return;
    }

    this.fireUpdateAction(animations.concat(addedAnimations));
  }

  onElementPickerStarted() {
    this.inspector.store.dispatch(updateElementPickerEnabled(true));
  }

  onElementPickerStopped() {
    this.inspector.store.dispatch(updateElementPickerEnabled(false));
  }

  onNavigate() {
    if (!this.isPanelVisible()) {
      return;
    }

    this.inspector.store.dispatch(updatePlaybackRates());
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
    } else {
      this.stopAnimationsCurrentTimeTimer();
      this.setAnimationStateChangedListenerEnabled(false);
    }
  }

  onSidebarResized(size) {
    if (!this.isPanelVisible()) {
      return;
    }

    this.inspector.store.dispatch(updateSidebarSize(size));
  }

  async onTargetAvailable({ targetFront }) {
    if (targetFront.isTopLevel) {
      this.animationsFront = await targetFront.getFront("animations");
      this.animationsFront.setWalkerActor(this.inspector.walker);
      this.animationsFront.on("mutations", this.onAnimationsMutation);

      await this.update();
    }
  }

  removeAnimationsCurrentTimeListener(listener) {
    this.animationsCurrentTimeListeners = this.animationsCurrentTimeListeners.filter(
      l => l !== listener
    );
  }

  async rewindAnimationsCurrentTime() {
    const { timeScale } = this.state;
    await this.setAnimationsCurrentTime(timeScale.zeroPositionTime, true);
  }

  selectAnimation(animation) {
    this.inspector.store.dispatch(updateSelectedAnimation(animation));
  }

  async setSelectedNode(nodeFront) {
    if (this.inspector.selection.nodeFront === nodeFront) {
      return;
    }

    await this.inspector
      .getCommonComponentProps()
      .setSelectedNode(nodeFront, { reason: "animation-panel" });
  }

  async setAnimationsCurrentTime(currentTime, shouldRefresh) {
    this.stopAnimationsCurrentTimeTimer();
    this.onAnimationsCurrentTimeUpdated(currentTime);

    if (!shouldRefresh && this.isCurrentTimeSet) {
      return;
    }

    let animations = this.state.animations;
    this.isCurrentTimeSet = true;

    try {
      await this.doSetCurrentTimes(currentTime);
      animations = await this.refreshAnimationsState(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    this.isCurrentTimeSet = false;

    if (shouldRefresh) {
      this.fireUpdateAction(animations);
    }
  }

  async setAnimationsPlaybackRate(playbackRate) {
    let animations = this.state.animations;
    // "changed" event on each animation will fire respectively when the playback
    // rate changed. Since for each occurrence of event, change of UI is urged.
    // To avoid this, disable the listeners once in order to not capture the event.
    this.setAnimationStateChangedListenerEnabled(false);

    try {
      await this.animationsFront.setPlaybackRates(animations, playbackRate);
      animations = await this.refreshAnimationsState(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    } finally {
      this.setAnimationStateChangedListenerEnabled(true);
    }

    await this.fireUpdateAction(animations);
  }

  async setAnimationsPlayState(doPlay) {
    let { animations, timeScale } = this.state;

    try {
      if (
        doPlay &&
        animations.every(
          animation =>
            timeScale.getEndTime(animation) <= animation.state.currentTime
        )
      ) {
        await this.doSetCurrentTimes(timeScale.zeroPositionTime);
      }

      if (doPlay) {
        await this.animationsFront.playSome(animations);
      } else {
        await this.animationsFront.pauseSome(animations);
      }

      animations = await this.refreshAnimationsState(animations);
    } catch (e) {
      // Expected if we've already been destroyed or other node have been selected
      // in the meantime.
      console.error(e);
      return;
    }

    await this.fireUpdateAction(animations);
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
   * Persistently highlight the given node identified with a unique selector.
   * If no node is provided, hide any persistent highlighter.
   *
   * @param {NodeFront} nodeFront
   */
  async setHighlightedNode(nodeFront) {
    await this.inspector.highlighters.hideHighlighterType(
      this.inspector.highlighters.TYPES.SELECTOR
    );

    if (nodeFront) {
      const selector = await nodeFront.getUniqueSelector();
      if (!selector) {
        console.warn(
          `Couldn't get unique selector for NodeFront: ${nodeFront.actorID}`
        );
        return;
      }

      /**
       * NOTE: Using a Selector Highlighter here because only one Box Model Highlighter
       * can be visible at a time. The Box Model Highlighter is shown when hovering nodes
       * which would cause this persistent highlighter to be hidden unexpectedly.
       * This limitation of one highlighter type a time should be solved by switching
       * to a highlighter by role approach (Bug 1663443).
       */
      await this.inspector.highlighters.showHighlighterTypeForNode(
        this.inspector.highlighters.TYPES.SELECTOR,
        nodeFront,
        {
          hideInfoBar: true,
          hideGuides: true,
          selector,
        }
      );
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

    this.simulatedAnimation.effect = new this.win.KeyframeEffect(
      targetEl,
      keyframes,
      effectTiming
    );

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

    this.simulatedAnimationForKeyframesProgressBar.effect = new this.win.KeyframeEffect(
      null,
      null,
      effectTiming
    );

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
    const shouldStopAfterEndTime = !hasAnimationIterationCountInfinite(
      this.state.animations
    );

    const currentTimeTimer = new CurrentTimeTimer(
      timeScale,
      shouldStopAfterEndTime,
      this.win,
      this.onCurrentTimeTimerUpdated
    );
    currentTimeTimer.start();
    this.currentTimeTimer = currentTimeTimer;
  }

  toggleElementPicker() {
    this.inspector.toolbox.nodePicker.togglePicker();
  }

  async update() {
    if (!this.isPanelVisible()) {
      return;
    }

    const done = this.inspector.updating("animationinspector");

    const selection = this.inspector.selection;
    const animations =
      selection.isConnected() && selection.isElementNode()
        ? await this.animationsFront.getAnimationPlayersForNode(
            selection.nodeFront
          )
        : [];
    this.fireUpdateAction(animations);
    this.setAnimationStateChangedListenerEnabled(true);

    done();
  }

  async refreshAnimationsState(animations) {
    let error = null;

    const promises = animations.map(animation => {
      return new Promise(resolve => {
        animation
          .refreshState()
          .catch(e => {
            error = e;
          })
          .finally(() => {
            resolve();
          });
      });
    });
    await Promise.all(promises);

    if (error) {
      throw new Error(error);
    }

    // Even when removal animation on inspected document, refreshAnimationsState
    // might be called before onAnimationsMutation due to the async timing.
    // Return the animations as result of refreshAnimationsState after getting rid of
    // the animations since they should not display.
    return animations.filter(anim => !!anim.state.type);
  }

  fireUpdateAction(animations) {
    // Animation inspector already destroyed
    if (!this.inspector) {
      return;
    }

    this.stopAnimationsCurrentTimeTimer();

    // Although it is not possible to set a delay or end delay of infinity using
    // the animation API, if the value passed exceeds the limit of our internal
    // representation of times, it will be treated as infinity. Rather than
    // adding special case code to represent this very rare case, we simply omit
    // such animations from the graph.
    animations = animations.filter(
      anim =>
        Math.abs(anim.state.delay) !== Infinity &&
        Math.abs(anim.state.endDelay) !== Infinity
    );

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
