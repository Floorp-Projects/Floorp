/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const {
  createNode,
  findOptimalTimeInterval,
  getFormattedAnimationTitle,
  TimeScale,
  getCssPropertyName
} = require("devtools/client/inspector/animation-old/utils");
const { AnimationDetails } = require("devtools/client/inspector/animation-old/components/animation-details");
const { AnimationTargetNode } = require("devtools/client/inspector/animation-old/components/animation-target-node");
const { AnimationTimeBlock } = require("devtools/client/inspector/animation-old/components/animation-time-block");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

// The minimum spacing between 2 time graduation headers in the timeline (px).
const TIME_GRADUATION_MIN_SPACING = 40;
// When the container window is resized, the timeline background gets refreshed,
// but only after a timer, and the timer is reset if the window is continuously
// resized.
const TIMELINE_BACKGROUND_RESIZE_DEBOUNCE_TIMER = 50;

/**
 * UI component responsible for displaying a timeline for animations.
 * The timeline is essentially a graph with time along the x axis and animations
 * along the y axis.
 * The time is represented with a graduation header at the top and a current
 * time play head.
 * Animations are organized by lines, with a left margin containing the preview
 * of the target DOM element the animation applies to.
 * The current time play head can be moved by clicking/dragging in the header.
 * when this happens, the component emits "current-data-changed" events with the
 * new time and state of the timeline.
 *
 * @param {InspectorPanel} inspector.
 * @param {Object} serverTraits The list of server-side capabilities.
 */
function AnimationsTimeline(inspector, serverTraits) {
  this.animations = [];
  this.componentsMap = {};
  this.inspector = inspector;
  this.serverTraits = serverTraits;

  this.onAnimationStateChanged = this.onAnimationStateChanged.bind(this);
  this.onScrubberMouseDown = this.onScrubberMouseDown.bind(this);
  this.onScrubberMouseUp = this.onScrubberMouseUp.bind(this);
  this.onScrubberMouseOut = this.onScrubberMouseOut.bind(this);
  this.onScrubberMouseMove = this.onScrubberMouseMove.bind(this);
  this.onAnimationSelected = this.onAnimationSelected.bind(this);
  this.onWindowResize = this.onWindowResize.bind(this);
  this.onTimelineDataChanged = this.onTimelineDataChanged.bind(this);
  this.onDetailCloseButtonClick = this.onDetailCloseButtonClick.bind(this);

  EventEmitter.decorate(this);
}

exports.AnimationsTimeline = AnimationsTimeline;

AnimationsTimeline.prototype = {
  init: function(containerEl) {
    this.win = containerEl.ownerDocument.defaultView;
    this.rootWrapperEl = containerEl;

    this.setupSplitBox();
    this.setupAnimationTimeline();
    this.setupAnimationDetail();

    this.win.addEventListener("resize",
      this.onWindowResize);
  },

  setupSplitBox: function() {
    const browserRequire = this.win.BrowserLoader({
      window: this.win,
      useOnlyShared: true
    }).require;

    const { createFactory } = browserRequire("devtools/client/shared/vendor/react");
    const dom = require("devtools/client/shared/vendor/react-dom-factories");
    const ReactDOM = browserRequire("devtools/client/shared/vendor/react-dom");

    const SplitBox = createFactory(
      browserRequire("devtools/client/shared/components/splitter/SplitBox"));

    const splitter = SplitBox({
      className: "animation-root",
      splitterSize: 1,
      initialHeight: "50%",
      endPanelControl: true,
      startPanel: dom.div({
        className: "animation-timeline"
      }),
      endPanel: dom.div({
        className: "animation-detail"
      }),
      vert: false
    });

    ReactDOM.render(splitter, this.rootWrapperEl);

    this.animationRootEl = this.rootWrapperEl.querySelector(".animation-root");
  },

  setupAnimationTimeline: function() {
    const animationTimelineEl = this.rootWrapperEl.querySelector(".animation-timeline");

    const scrubberContainer = createNode({
      parent: animationTimelineEl,
      attributes: {"class": "scrubber-wrapper"}
    });

    this.scrubberEl = createNode({
      parent: scrubberContainer,
      attributes: {
        "class": "scrubber"
      }
    });

    this.scrubberHandleEl = createNode({
      parent: this.scrubberEl,
      attributes: {
        "class": "scrubber-handle"
      }
    });
    createNode({
      parent: this.scrubberHandleEl,
      attributes: {
        "class": "scrubber-line"
      }
    });
    this.scrubberHandleEl.addEventListener("mousedown",
                                           this.onScrubberMouseDown);

    this.headerWrapper = createNode({
      parent: animationTimelineEl,
      attributes: {
        "class": "header-wrapper"
      }
    });

    this.timeHeaderEl = createNode({
      parent: this.headerWrapper,
      attributes: {
        "class": "time-header track-container"
      }
    });

    this.timeHeaderEl.addEventListener("mousedown",
                                       this.onScrubberMouseDown);

    this.timeTickEl = createNode({
      parent: animationTimelineEl,
      attributes: {
        "class": "time-body track-container"
      }
    });

    this.animationsEl = createNode({
      parent: animationTimelineEl,
      nodeType: "ul",
      attributes: {
        "class": "animations devtools-monospace"
      }
    });
  },

  setupAnimationDetail: function() {
    const animationDetailEl = this.rootWrapperEl.querySelector(".animation-detail");

    const animationDetailHeaderEl = createNode({
      parent: animationDetailEl,
      attributes: {
        "class": "animation-detail-header"
      }
    });

    const headerTitleEl = createNode({
      parent: animationDetailHeaderEl,
      attributes: {
        "class": "devtools-toolbar"
      }
    });

    createNode({
      parent: headerTitleEl,
      textContent: L10N.getStr("detail.headerTitle")
    });

    this.animationAnimationNameEl = createNode({
      parent: headerTitleEl
    });

    this.animationDetailCloseButton = createNode({
      parent: headerTitleEl,
      nodeType: "button",
      attributes: {
        "class": "devtools-button",
        title: L10N.getStr("detail.header.closeLabel"),
      }
    });
    this.animationDetailCloseButton.addEventListener("click",
                                                     this.onDetailCloseButtonClick);

    const animationDetailBodyEl = createNode({
      parent: animationDetailEl,
      attributes: {
        "class": "animation-detail-body"
      }
    });

    this.animatedPropertiesEl = createNode({
      parent: animationDetailBodyEl,
      attributes: {
        "class": "animated-properties"
      }
    });

    this.details = new AnimationDetails(this.serverTraits);
    this.details.init(this.animatedPropertiesEl);
  },

  destroy: function() {
    this.stopAnimatingScrubber();
    this.unrender();
    this.details.destroy();

    this.win.removeEventListener("resize",
      this.onWindowResize);
    this.timeHeaderEl.removeEventListener("mousedown",
      this.onScrubberMouseDown);
    this.scrubberHandleEl.removeEventListener("mousedown",
      this.onScrubberMouseDown);
    this.animationDetailCloseButton.removeEventListener("click",
      this.onDetailCloseButtonClick);

    this.rootWrapperEl.remove();
    this.animations = [];
    this.componentsMap = null;
    this.rootWrapperEl = null;
    this.timeHeaderEl = null;
    this.animationsEl = null;
    this.animatedPropertiesEl = null;
    this.scrubberEl = null;
    this.scrubberHandleEl = null;
    this.win = null;
    this.inspector = null;
    this.serverTraits = null;
    this.animationDetailEl = null;
    this.animationAnimationNameEl = null;
    this.animatedPropertiesEl = null;
    this.animationDetailCloseButton = null;
    this.animationRootEl = null;
    this.selectedAnimation = null;

    this.isDestroyed = true;
  },

  /**
   * Destroy all sub-components that have been created and stored on this instance.
   */
  destroyAllSubComponents: function() {
    for (const actorID in this.componentsMap) {
      this.destroySubComponents(actorID);
    }
  },

  /**
   * Destroy sub-components which related to given actor id.
   * @param {String} actor id
   */
  destroySubComponents: function(actorID) {
    const components = this.componentsMap[actorID];
    components.timeBlock.destroy();
    components.targetNode.destroy();
    components.animationEl.remove();
    delete components.state;
    delete components.tracks;
    delete this.componentsMap[actorID];
  },

  unrender: function() {
    for (const animation of this.animations) {
      animation.off("changed", this.onAnimationStateChanged);
    }
    this.stopAnimatingScrubber();
    TimeScale.reset();
    this.destroyAllSubComponents();
    this.animationsEl.innerHTML = "";
    this.off("timeline-data-changed", this.onTimelineDataChanged);
    this.details.unrender();
  },

  onWindowResize: function() {
    // Don't do anything if the root element has a width of 0
    if (this.rootWrapperEl.offsetWidth === 0) {
      return;
    }

    if (this.windowResizeTimer) {
      this.win.clearTimeout(this.windowResizeTimer);
    }

    this.windowResizeTimer = this.win.setTimeout(() => {
      this.drawHeaderAndBackground();
    }, TIMELINE_BACKGROUND_RESIZE_DEBOUNCE_TIMER);
  },

  async onAnimationSelected(animation) {
    const index = this.animations.indexOf(animation);
    if (index === -1) {
      return;
    }

    // Unselect an animation which was selected.
    const animationEls = this.rootWrapperEl.querySelectorAll(".animation");
    for (let i = 0; i < animationEls.length; i++) {
      const animationEl = animationEls[i];
      if (!animationEl.classList.contains("selected")) {
        continue;
      }
      if (i === index) {
        if (this.animationRootEl.classList.contains("animation-detail-visible")) {
          // Already the animation is selected.
          this.emit("animation-already-selected", this.animations[i]);
          return;
        }
      } else {
        animationEl.classList.remove("selected");
        this.emit("animation-unselected", this.animations[i]);
      }
      break;
    }

    // Add class of animation type to animatedPropertiesEl to display the compositor sign.
    if (!this.animatedPropertiesEl.classList.contains(animation.state.type)) {
      this.animatedPropertiesEl.className =
        `animated-properties ${ animation.state.type }`;
    }

    // Select and render.
    const selectedAnimationEl = animationEls[index];
    selectedAnimationEl.classList.add("selected");
    this.animationRootEl.classList.add("animation-detail-visible");
    // Don't render if the detail displays same animation already.
    if (animation !== this.details.animation) {
      this.selectedAnimation = animation;
      await this.details.render(animation, this.componentsMap[animation.actorID].tracks);
      this.animationAnimationNameEl.textContent = getFormattedAnimationTitle(animation);
    }
    this.onTimelineDataChanged({ time: this.currentTime || 0 });
    this.emit("animation-selected", animation);
  },

  /**
   * When move the scrubber to the corresponding position
   */
  onScrubberMouseDown: function(e) {
    this.moveScrubberTo(e.pageX);
    this.win.addEventListener("mouseup", this.onScrubberMouseUp);
    this.win.addEventListener("mouseout", this.onScrubberMouseOut);
    this.win.addEventListener("mousemove", this.onScrubberMouseMove);

    // Prevent text selection while dragging.
    e.preventDefault();
  },

  onScrubberMouseUp: function() {
    this.cancelTimeHeaderDragging();
  },

  onScrubberMouseOut: function(e) {
    // Check that mouseout happened on the window itself, and if yes, cancel
    // the dragging.
    if (!this.win.document.contains(e.relatedTarget)) {
      this.cancelTimeHeaderDragging();
    }
  },

  cancelTimeHeaderDragging: function() {
    this.win.removeEventListener("mouseup", this.onScrubberMouseUp);
    this.win.removeEventListener("mouseout", this.onScrubberMouseOut);
    this.win.removeEventListener("mousemove", this.onScrubberMouseMove);
  },

  onScrubberMouseMove: function(e) {
    this.moveScrubberTo(e.pageX);
  },

  moveScrubberTo: function(pageX, noOffset) {
    this.stopAnimatingScrubber();

    // The offset needs to be in % and relative to the timeline's area (so we
    // subtract the scrubber's left offset, which is equal to the sidebar's
    // width).
    let offset = pageX;
    if (!noOffset) {
      offset -= this.timeHeaderEl.offsetLeft;
    }
    offset = offset * 100 / this.timeHeaderEl.offsetWidth;
    if (offset < 0) {
      offset = 0;
    }

    this.scrubberEl.style.left = offset + "%";

    const time = TimeScale.distanceToRelativeTime(offset);

    this.emit("timeline-data-changed", {
      isPaused: true,
      isMoving: false,
      isUserDrag: true,
      time: time
    });
  },

  getCompositorStatusClassName: function(state) {
    let className = state.isRunningOnCompositor
                    ? " fast-track"
                    : "";

    if (state.isRunningOnCompositor && state.propertyState) {
      className +=
        state.propertyState.some(propState => !propState.runningOnCompositor)
        ? " some-properties"
        : " all-properties";
    }

    return className;
  },

  async render(animations, documentCurrentTime) {
    this.animations = animations;

    // Destroy components which are no longer existed in given animations.
    for (const animation of this.animations) {
      if (this.componentsMap[animation.actorID]) {
        this.componentsMap[animation.actorID].needToLeave = true;
      }
    }
    for (const actorID in this.componentsMap) {
      const components = this.componentsMap[actorID];
      if (components.needToLeave) {
        delete components.needToLeave;
      } else {
        this.destroySubComponents(actorID);
      }
    }

    if (!this.animations.length) {
      this.emit("animation-timeline-rendering-completed");
      return;
    }

    // Loop to set the time scale for all current animations.
    TimeScale.reset();
    for (const {state} of animations) {
      TimeScale.addAnimation(state);
    }

    this.drawHeaderAndBackground();

    for (const animation of this.animations) {
      animation.on("changed", this.onAnimationStateChanged);

      const tracks = await this.getTracks(animation);
      // If we're destroyed by now, just give up.
      if (this.isDestroyed) {
        return;
      }

      if (this.componentsMap[animation.actorID]) {
        // Update animation UI using existent components.
        this.updateAnimation(animation, tracks, this.componentsMap[animation.actorID]);
      } else {
        // Render animation UI as new element.
        const animationEl = createNode({
          parent: this.animationsEl,
          nodeType: "li",
        });
        this.renderAnimation(animation, tracks, animationEl);
      }
    }

    // Use the document's current time to position the scrubber (if the server
    // doesn't provide it, hide the scrubber entirely).
    // Note that because the currentTime was sent via the protocol, some time
    // may have gone by since then, and so the scrubber might be a bit late.
    if (!documentCurrentTime) {
      this.scrubberEl.style.display = "none";
    } else {
      this.scrubberEl.style.display = "block";
      this.startAnimatingScrubber(this.wasRewound()
                                  ? TimeScale.minStartTime
                                  : documentCurrentTime);
    }

    // To indicate the animation progress in AnimationDetails.
    this.on("timeline-data-changed", this.onTimelineDataChanged);

    if (this.animations.length === 1) {
      // Display animation's detail if there is only one animation,
      // even if the detail pane is closing.
      await this.onAnimationSelected(this.animations[0]);
    } else if (this.animationRootEl.classList.contains("animation-detail-visible") &&
               this.animations.includes(this.selectedAnimation)) {
      // animation's detail displays in case of the previously displayed animation is
      // included in timeline list and the detail pane is not closing.
      await this.onAnimationSelected(this.selectedAnimation);
    } else {
      // Otherwise, close detail pane.
      this.onDetailCloseButtonClick();
    }
    this.emit("animation-timeline-rendering-completed");
  },

  updateAnimation: function(animation, tracks, existentComponents) {
    // If keyframes (tracks) and effect timing (state) are not changed, we update the
    // view box only.
    // As an exception, if iterationCount reprensents Infinity, we need to re-render
    // the shape along new time scale.
    // FIXME: To avoid re-rendering even Infinity, we need to change the
    // representation for Infinity.
    if (animation.state.iterationCount &&
        areTimingEffectsEqual(existentComponents.state, animation.state) &&
        existentComponents.tracks.toString() === tracks.toString()) {
      // Update timeBlock.
      existentComponents.timeBlock.update(animation);
    } else {
      // Destroy previous components.
      existentComponents.timeBlock.destroy();
      existentComponents.targetNode.destroy();
      // Remove children to re-use.
      existentComponents.animationEl.innerHTML = "";
      // Re-render animation using existent animationEl.
      this.renderAnimation(animation, tracks, existentComponents.animationEl);
    }
  },

  renderAnimation: function(animation, tracks, animationEl) {
    animationEl.setAttribute("class",
                             "animation " + animation.state.type +
                             this.getCompositorStatusClassName(animation.state));

    // Left sidebar for the animated node.
    const animatedNodeEl = createNode({
      parent: animationEl,
      attributes: {
        "class": "target"
      }
    });

    // Draw the animated node target.
    const targetNode = new AnimationTargetNode(this.inspector, {compact: true});
    targetNode.init(animatedNodeEl);
    targetNode.render(animation);

    // Right-hand part contains the timeline itself (called time-block here).
    const timeBlockEl = createNode({
      parent: animationEl,
      attributes: {
        "class": "time-block track-container"
      }
    });

    // Draw the animation time block.
    const timeBlock = new AnimationTimeBlock();
    timeBlock.init(timeBlockEl);
    timeBlock.render(animation, tracks);
    timeBlock.on("selected", this.onAnimationSelected);

    this.componentsMap[animation.actorID] = {
      animationEl, targetNode, timeBlock, tracks, state: animation.state
    };
  },

  isAtLeastOneAnimationPlaying: function() {
    return this.animations.some(({state}) => state.playState === "running");
  },

  wasRewound: function() {
    return !this.isAtLeastOneAnimationPlaying() &&
           this.animations.every(({state}) => state.currentTime === 0);
  },

  hasInfiniteAnimations: function() {
    return this.animations.some(({state}) => !state.iterationCount);
  },

  startAnimatingScrubber: function(time) {
    const isOutOfBounds = time < TimeScale.minStartTime ||
                        time > TimeScale.maxEndTime;
    const isAllPaused = !this.isAtLeastOneAnimationPlaying();
    const hasInfinite = this.hasInfiniteAnimations();

    let x = TimeScale.startTimeToDistance(time);
    if (x > 100 && !hasInfinite) {
      x = 100;
    }
    this.scrubberEl.style.left = x + "%";

    // Only stop the scrubber if it's out of bounds or all animations have been
    // paused, but not if at least an animation is infinite.
    if (isAllPaused || (isOutOfBounds && !hasInfinite)) {
      this.stopAnimatingScrubber();
      this.emit("timeline-data-changed", {
        isPaused: !this.isAtLeastOneAnimationPlaying(),
        isMoving: false,
        isUserDrag: false,
        time: TimeScale.distanceToRelativeTime(x)
      });
      return;
    }

    this.emit("timeline-data-changed", {
      isPaused: false,
      isMoving: true,
      isUserDrag: false,
      time: TimeScale.distanceToRelativeTime(x)
    });

    const now = this.win.performance.now();
    this.rafID = this.win.requestAnimationFrame(() => {
      if (!this.rafID) {
        // In case the scrubber was stopped in the meantime.
        return;
      }
      this.startAnimatingScrubber(time + this.win.performance.now() - now);
    });
  },

  stopAnimatingScrubber: function() {
    if (this.rafID) {
      this.win.cancelAnimationFrame(this.rafID);
      this.rafID = null;
    }
  },

  onAnimationStateChanged: function() {
    // For now, simply re-render the component. The animation front's state has
    // already been updated.
    this.render(this.animations);
  },

  drawHeaderAndBackground: function() {
    const width = this.timeHeaderEl.offsetWidth;
    const animationDuration = TimeScale.maxEndTime - TimeScale.minStartTime;
    const minTimeInterval = TIME_GRADUATION_MIN_SPACING *
                          animationDuration / width;
    const intervalLength = findOptimalTimeInterval(minTimeInterval);
    const intervalWidth = intervalLength * width / animationDuration;

    // And the time graduation header.
    this.timeHeaderEl.innerHTML = "";
    this.timeTickEl.innerHTML = "";

    for (let i = 0; i <= width / intervalWidth; i++) {
      const pos = 100 * i * intervalWidth / width;

      // This element is the header of time tick for displaying animation
      // duration time.
      createNode({
        parent: this.timeHeaderEl,
        nodeType: "span",
        attributes: {
          "class": "header-item",
          "style": `left:${pos}%`
        },
        textContent: TimeScale.formatTime(TimeScale.distanceToRelativeTime(pos))
      });

      // This element is displayed as a vertical line separator corresponding
      // the header of time tick for indicating time slice for animation
      // iterations.
      createNode({
        parent: this.timeTickEl,
        nodeType: "span",
        attributes: {
          "class": "time-tick",
          "style": `left:${pos}%`
        }
      });
    }
  },

  onTimelineDataChanged: function({ time }) {
    this.currentTime = time;
    const indicateTime =
      TimeScale.minStartTime === Infinity ? 0 : this.currentTime + TimeScale.minStartTime;
    this.details.indicateProgress(indicateTime);
  },

  onDetailCloseButtonClick: function(e) {
    if (!this.animationRootEl.classList.contains("animation-detail-visible")) {
      return;
    }
    this.animationRootEl.classList.remove("animation-detail-visible");
    this.emit("animation-detail-closed");
  },

  /**
   * Get a list of the tracks of the animation actor
   * @param {Object} animation
   * @return {Object} A list of tracks, one per animated property, each
   * with a list of keyframes
   */
  async getTracks(animation) {
    const tracks = {};

    /*
     * getFrames is a AnimationPlayorActor method that returns data about the
     * keyframes of the animation.
     * In FF48, the data it returns change, and will hold only longhand
     * properties ( e.g. borderLeftWidth ), which does not match what we
     * want to display in the animation detail.
     * A new AnimationPlayerActor function, getProperties, is introduced,
     * that returns the animated css properties of the animation and their
     * keyframes values.
     * If the animation actor has the getProperties function, we use it, and if
     * not, we fall back to getFrames, which then returns values we used to
     * handle.
     */
    if (this.serverTraits.hasGetProperties) {
      let properties = [];
      try {
        properties = await animation.getProperties();
      } catch (e) {
        // Expected if we've already been destroyed in the meantime.
        if (!this.isDestroyed) {
          throw e;
        }
      }

      for (const {name, values} of properties) {
        if (!tracks[name]) {
          tracks[name] = [];
        }

        for (let {value, offset, easing, distance} of values) {
          distance = distance ? distance : 0;
          offset = parseFloat(offset.toFixed(3));
          tracks[name].push({value, offset, easing, distance});
        }
      }
    } else {
      let frames = [];
      try {
        frames = await animation.getFrames();
      } catch (e) {
        // Expected if we've already been destroyed in the meantime.
        if (!this.isDestroyed) {
          throw e;
        }
      }

      for (const frame of frames) {
        for (const name in frame) {
          if (this.NON_PROPERTIES.includes(name)) {
            continue;
          }

          // We have to change to CSS property name
          // since GetKeyframes returns JS property name.
          const propertyCSSName = getCssPropertyName(name);
          if (!tracks[propertyCSSName]) {
            tracks[propertyCSSName] = [];
          }

          tracks[propertyCSSName].push({
            value: frame[name],
            offset: parseFloat(frame.computedOffset.toFixed(3)),
            easing: frame.easing,
            distance: 0
          });
        }
      }
    }

    return tracks;
  }
};

/**
 * Check the equality given states as effect timing.
 * @param {Object} state of animation.
 * @param {Object} same to avobe.
 * @return {boolean} true: same effect timing
 */
function areTimingEffectsEqual(stateA, stateB) {
  for (const property of ["playbackRate", "duration", "delay", "endDelay",
                          "iterationCount", "iterationStart", "easing",
                          "fill", "direction"]) {
    if (stateA[property] !== stateB[property]) {
      return false;
    }
  }
  return true;
}
