/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals ViewHelpers */

"use strict";

// Set of reusable UI components for the animation-inspector UI.
// All components in this module share a common API:
// 1. construct the component:
//    let c = new ComponentName();
// 2. initialize the markup of the component in a given parent node:
//    c.init(containerElement);
// 3. render the component, passing in some sort of state:
//    This may be called over and over again when the state changes, to update
//    the component output.
//    c.render(state);
// 4. destroy the component:
//    c.destroy();

const {Cu} = require("chrome");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {
  createNode,
  drawGraphElementBackground,
  findOptimalTimeInterval
} = require("devtools/animationinspector/utils");

const STRINGS_URI = "chrome://browser/locale/devtools/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const MILLIS_TIME_FORMAT_MAX_DURATION = 4000;
// The minimum spacing between 2 time graduation headers in the timeline (px).
const TIME_GRADUATION_MIN_SPACING = 40;

/**
 * UI component responsible for displaying and updating the player meta-data:
 * name, duration, iterations, delay.
 * The parent UI component for this should drive its updates by calling
 * render(state) whenever it wants the component to update.
 */
function PlayerMetaDataHeader() {
  // Store the various state pieces we need to only refresh the UI when things
  // change.
  this.state = {};
}

exports.PlayerMetaDataHeader = PlayerMetaDataHeader;

PlayerMetaDataHeader.prototype = {
  init: function(containerEl) {
    // The main title element.
    this.el = createNode({
      parent: containerEl,
      attributes: {
        "class": "animation-title"
      }
    });

    // Animation name.
    this.nameLabel = createNode({
      parent: this.el,
      nodeType: "span"
    });

    this.nameValue = createNode({
      parent: this.el,
      nodeType: "strong",
      attributes: {
        "style": "display:none;"
      }
    });

    // Animation duration, delay and iteration container.
    let metaData = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "class": "meta-data"
      }
    });

    // Animation is running on compositor
    this.compositorIcon = createNode({
      parent: metaData,
      nodeType: "span",
      attributes: {
        "class": "compositor-icon",
        "title": L10N.getStr("player.runningOnCompositorTooltip")
      }
    });

    // Animation duration.
    this.durationLabel = createNode({
      parent: metaData,
      nodeType: "span",
      textContent: L10N.getStr("player.animationDurationLabel")
    });

    this.durationValue = createNode({
      parent: metaData,
      nodeType: "strong"
    });

    // Animation delay (hidden by default since there may not be a delay).
    this.delayLabel = createNode({
      parent: metaData,
      nodeType: "span",
      attributes: {
        "style": "display:none;"
      },
      textContent: L10N.getStr("player.animationDelayLabel")
    });

    this.delayValue = createNode({
      parent: metaData,
      nodeType: "strong"
    });

    // Animation iteration count (also hidden by default since we don't display
    // single iterations).
    this.iterationLabel = createNode({
      parent: metaData,
      nodeType: "span",
      attributes: {
        "style": "display:none;"
      },
      textContent: L10N.getStr("player.animationIterationCountLabel")
    });

    this.iterationValue = createNode({
      parent: metaData,
      nodeType: "strong",
      attributes: {
        "style": "display:none;"
      }
    });
  },

  destroy: function() {
    this.state = null;
    this.el.remove();
    this.el = null;
    this.nameLabel = this.nameValue = null;
    this.durationLabel = this.durationValue = null;
    this.delayLabel = this.delayValue = null;
    this.iterationLabel = this.iterationValue = null;
    this.compositorIcon = null;
  },

  render: function(state) {
    // Update the name if needed.
    if (state.name !== this.state.name) {
      if (state.name) {
        // Animations (and transitions since bug 1122414) have names.
        this.nameLabel.textContent = L10N.getStr("player.animationNameLabel");
        this.nameValue.style.display = "inline";
        this.nameValue.textContent = state.name;
      } else {
        // With older actors, Css transitions don't have names.
        this.nameLabel.textContent = L10N.getStr("player.transitionNameLabel");
        this.nameValue.style.display = "none";
      }
    }

    // update the duration value if needed.
    if (state.duration !== this.state.duration) {
      this.durationValue.textContent = L10N.getFormatStr("player.timeLabel",
        L10N.numberWithDecimals(state.duration / 1000, 2));
    }

    // Update the delay if needed.
    if (state.delay !== this.state.delay) {
      if (state.delay) {
        this.delayLabel.style.display = "inline";
        this.delayValue.style.display = "inline";
        this.delayValue.textContent = L10N.getFormatStr("player.timeLabel",
          L10N.numberWithDecimals(state.delay / 1000, 2));
      } else {
        // Hide the delay elements if there is no delay defined.
        this.delayLabel.style.display = "none";
        this.delayValue.style.display = "none";
      }
    }

    // Update the iterationCount if needed.
    if (state.iterationCount !== this.state.iterationCount) {
      if (state.iterationCount !== 1) {
        this.iterationLabel.style.display = "inline";
        this.iterationValue.style.display = "inline";
        let count = state.iterationCount ||
                    L10N.getStr("player.infiniteIterationCount");
        this.iterationValue.innerHTML = count;
      } else {
        // Hide the iteration elements if iteration is 1.
        this.iterationLabel.style.display = "none";
        this.iterationValue.style.display = "none";
      }
    }

    // Show the Running on compositor icon if needed.
    if (state.isRunningOnCompositor !== this.state.isRunningOnCompositor) {
      if (state.isRunningOnCompositor) {
        this.compositorIcon.style.display = "inline";
      } else {
        // Hide the compositor icon
        this.compositorIcon.style.display = "none";
      }
    }

    this.state = state;
  }
};

/**
 * UI component responsible for displaying the playback rate drop-down in each
 * player widget, updating it when the state changes, and emitting events when
 * the user selects a new value.
 * The parent UI component for this should drive its updates by calling
 * render(state) whenever it wants the component to update.
 */
function PlaybackRateSelector() {
  this.currentRate = null;
  this.onSelectionChanged = this.onSelectionChanged.bind(this);
  EventEmitter.decorate(this);
}

exports.PlaybackRateSelector = PlaybackRateSelector;

PlaybackRateSelector.prototype = {
  PRESETS: [.1, .5, 1, 2, 5, 10],

  init: function(containerEl) {
    // This component is simple enough that we can re-create the markup every
    // time it's rendered. So here we only store the parentEl.
    this.parentEl = containerEl;
  },

  destroy: function() {
    this.removeSelect();
    this.parentEl = this.el = null;
  },

  removeSelect: function() {
    if (this.el) {
      this.el.removeEventListener("change", this.onSelectionChanged);
      this.el.remove();
    }
  },

  /**
   * Get the ordered list of presets, including the current playbackRate if
   * different from the existing presets.
   */
  getCurrentPresets: function({playbackRate}) {
    return [...new Set([...this.PRESETS, playbackRate])].sort((a, b) => a > b);
  },

  render: function(state) {
    if (state.playbackRate === this.currentRate) {
      return;
    }

    this.removeSelect();

    this.el = createNode({
      parent: this.parentEl,
      nodeType: "select",
      attributes: {
        "class": "rate devtools-button"
      }
    });

    for (let preset of this.getCurrentPresets(state)) {
      let option = createNode({
        parent: this.el,
        nodeType: "option",
        attributes: {
          value: preset,
        },
        textContent: L10N.getFormatStr("player.playbackRateLabel", preset)
      });
      if (preset === state.playbackRate) {
        option.setAttribute("selected", "");
      }
    }

    this.el.addEventListener("change", this.onSelectionChanged);

    this.currentRate = state.playbackRate;
  },

  onSelectionChanged: function() {
    this.emit("rate-changed", parseFloat(this.el.value));
  }
};

/**
 * UI component responsible for displaying a preview of the target dom node of
 * a given animation.
 * @param {InspectorPanel} inspector Requires a reference to the inspector-panel
 * to highlight and select the node, as well as refresh it when there are
 * mutations.
 * @param {Object} options Supported properties are:
 * - compact {Boolean} Defaults to false. If true, nodes will be previewed like
 *   tag#id.class instead of <tag id="id" class="class">
 */
function AnimationTargetNode(inspector, options={}) {
  this.inspector = inspector;
  this.options = options;

  this.onPreviewMouseOver = this.onPreviewMouseOver.bind(this);
  this.onPreviewMouseOut = this.onPreviewMouseOut.bind(this);
  this.onSelectNodeClick = this.onSelectNodeClick.bind(this);
  this.onMarkupMutations = this.onMarkupMutations.bind(this);

  EventEmitter.decorate(this);
}

exports.AnimationTargetNode = AnimationTargetNode;

AnimationTargetNode.prototype = {
  init: function(containerEl) {
    let document = containerEl.ownerDocument;

    // Init the markup for displaying the target node.
    this.el = createNode({
      parent: containerEl,
      attributes: {
        "class": "animation-target"
      }
    });

    // Icon to select the node in the inspector.
    this.selectNodeEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "class": "node-selector"
      }
    });

    // Wrapper used for mouseover/out event handling.
    this.previewEl = createNode({
      parent: this.el,
      nodeType: "span"
    });

    if (!this.options.compact) {
      this.previewEl.appendChild(document.createTextNode("<"));
    }

    // Tag name.
    this.tagNameEl = createNode({
      parent: this.previewEl,
      nodeType: "span",
      attributes: {
        "class": "tag-name theme-fg-color3"
      }
    });

    // Id attribute container.
    this.idEl = createNode({
      parent: this.previewEl,
      nodeType: "span"
    });

    if (!this.options.compact) {
      createNode({
        parent: this.idEl,
        nodeType: "span",
        attributes: {
          "class": "attribute-name theme-fg-color2"
        },
        textContent: "id"
      });
      this.idEl.appendChild(document.createTextNode("=\""));
    } else {
      createNode({
        parent: this.idEl,
        nodeType: "span",
        attributes: {
          "class": "theme-fg-color2"
        },
        textContent: "#"
      });
    }

    createNode({
      parent: this.idEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color6"
      }
    });

    if (!this.options.compact) {
      this.idEl.appendChild(document.createTextNode("\""));
    }

    // Class attribute container.
    this.classEl = createNode({
      parent: this.previewEl,
      nodeType: "span"
    });

    if (!this.options.compact) {
      createNode({
        parent: this.classEl,
        nodeType: "span",
        attributes: {
          "class": "attribute-name theme-fg-color2"
        },
        textContent: "class"
      });
      this.classEl.appendChild(document.createTextNode("=\""));
    } else {
      createNode({
        parent: this.classEl,
        nodeType: "span",
        attributes: {
          "class": "theme-fg-color6"
        },
        textContent: "."
      });
    }

    createNode({
      parent: this.classEl,
      nodeType: "span",
      attributes: {
        "class": "attribute-value theme-fg-color6"
      }
    });

    if (!this.options.compact) {
      this.classEl.appendChild(document.createTextNode("\""));
      this.previewEl.appendChild(document.createTextNode(">"));
    }

    // Init events for highlighting and selecting the node.
    this.previewEl.addEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.addEventListener("mouseout", this.onPreviewMouseOut);
    this.selectNodeEl.addEventListener("click", this.onSelectNodeClick);

    // Start to listen for markupmutation events.
    this.inspector.on("markupmutation", this.onMarkupMutations);
  },

  destroy: function() {
    this.inspector.off("markupmutation", this.onMarkupMutations);
    this.previewEl.removeEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.removeEventListener("mouseout", this.onPreviewMouseOut);
    this.selectNodeEl.removeEventListener("click", this.onSelectNodeClick);
    this.el.remove();
    this.el = this.tagNameEl = this.idEl = this.classEl = null;
    this.selectNodeEl = this.previewEl = null;
    this.nodeFront = this.inspector = this.playerFront = null;
  },

  onPreviewMouseOver: function() {
    if (!this.nodeFront) {
      return;
    }
    this.inspector.toolbox.highlighterUtils.highlightNodeFront(this.nodeFront);
  },

  onPreviewMouseOut: function() {
    this.inspector.toolbox.highlighterUtils.unhighlight();
  },

  onSelectNodeClick: function() {
    if (!this.nodeFront) {
      return;
    }
    this.inspector.selection.setNodeFront(this.nodeFront, "animationinspector");
  },

  onMarkupMutations: function(e, mutations) {
    if (!this.nodeFront || !this.playerFront) {
      return;
    }

    for (let {target} of mutations) {
      if (target === this.nodeFront) {
        // Re-render with the same nodeFront to update the output.
        this.render(this.playerFront);
        break;
      }
    }
  },

  render: Task.async(function*(playerFront) {
    this.playerFront = playerFront;
    this.nodeFront = undefined;

    try {
      this.nodeFront = yield this.inspector.walker.getNodeFromActor(
                             playerFront.actorID, ["node"]);
    } catch (e) {
      // We might have been destroyed in the meantime, or the node might not be
      // found.
      if (!this.el) {
        console.warn("Cound't retrieve the animation target node, widget " +
                     "destroyed");
      }
      console.error(e);
      return;
    }

    if (!this.nodeFront || !this.el) {
      return;
    }

    let {tagName, attributes} = this.nodeFront;

    this.tagNameEl.textContent = tagName.toLowerCase();

    let idIndex = attributes.findIndex(({name}) => name === "id");
    if (idIndex > -1 && attributes[idIndex].value) {
      this.idEl.querySelector(".attribute-value").textContent =
        attributes[idIndex].value;
      this.idEl.style.display = "inline";
    } else {
      this.idEl.style.display = "none";
    }

    let classIndex = attributes.findIndex(({name}) => name === "class");
    if (classIndex > -1 && attributes[classIndex].value) {
      let value = attributes[classIndex].value;
      if (this.options.compact) {
        value = value.split(" ").join(".");
      }

      this.classEl.querySelector(".attribute-value").textContent = value;
      this.classEl.style.display = "inline";
    } else {
      this.classEl.style.display = "none";
    }

    this.emit("target-retrieved");
  })
};

/**
 * The TimeScale helper object is used to know which size should something be
 * displayed with in the animation panel, depending on the animations that are
 * currently displayed.
 * If there are 5 animations displayed, and the first one starts at 10000ms and
 * the last one ends at 20000ms, then this helper can be used to convert any
 * time in this range to a distance in pixels.
 *
 * For the helper to know how to convert, it needs to know all the animations.
 * Whenever a new animation is added to the panel, addAnimation(state) should be
 * called. reset() can be called to start over.
 */
let TimeScale = {
  minStartTime: Infinity,
  maxEndTime: 0,

  /**
   * Add a new animation to time scale.
   * @param {Object} state A PlayerFront.state object.
   */
  addAnimation: function(state) {
    let {startTime, delay, duration, iterationCount, playbackRate} = state;

    // Negative-delayed animations have their startTimes set such that we would
    // be displaying the delay outside the time window if we didn't take it into
    // account here.
    let relevantDelay = delay < 0 ? delay / playbackRate : 0;

    this.minStartTime = Math.min(this.minStartTime, startTime + relevantDelay);
    let length = (delay / playbackRate) +
                 ((duration / playbackRate) *
                  (!iterationCount ? 1 : iterationCount));
    this.maxEndTime = Math.max(this.maxEndTime, startTime + length);
  },

  /**
   * Reset the current time scale.
   */
  reset: function() {
    this.minStartTime = Infinity;
    this.maxEndTime = 0;
  },

  /**
   * Convert a startTime to a distance in pixels, in the current time scale.
   * @param {Number} time
   * @param {Number} containerWidth The width of the container element.
   * @return {Number}
   */
  startTimeToDistance: function(time, containerWidth) {
    time -= this.minStartTime;
    return this.durationToDistance(time, containerWidth);
  },

  /**
   * Convert a duration to a distance in pixels, in the current time scale.
   * @param {Number} time
   * @param {Number} containerWidth The width of the container element.
   * @return {Number}
   */
  durationToDistance: function(duration, containerWidth) {
    return containerWidth * duration / (this.maxEndTime - this.minStartTime);
  },

  /**
   * Convert a distance in pixels to a time, in the current time scale.
   * @param {Number} distance
   * @param {Number} containerWidth The width of the container element.
   * @return {Number}
   */
  distanceToTime: function(distance, containerWidth) {
    return this.minStartTime +
      ((this.maxEndTime - this.minStartTime) * distance / containerWidth);
  },

  /**
   * Convert a distance in pixels to a time, in the current time scale.
   * The time will be relative to the current minimum start time.
   * @param {Number} distance
   * @param {Number} containerWidth The width of the container element.
   * @return {Number}
   */
  distanceToRelativeTime: function(distance, containerWidth) {
    let time = this.distanceToTime(distance, containerWidth);
    return time - this.minStartTime;
  },

  /**
   * Depending on the time scale, format the given time as milliseconds or
   * seconds.
   * @param {Number} time
   * @return {String} The formatted time string.
   */
  formatTime: function(time) {
    let duration = this.maxEndTime - this.minStartTime;

    // Format in milliseconds if the total duration is short enough.
    if (duration <= MILLIS_TIME_FORMAT_MAX_DURATION) {
      return L10N.getFormatStr("timeline.timeGraduationLabel", time.toFixed(0));
    }

    // Otherwise format in seconds.
    return L10N.getFormatStr("player.timeLabel", (time / 1000).toFixed(1));
  }
};

exports.TimeScale = TimeScale;

/**
 * UI component responsible for displaying a timeline for animations.
 * The timeline is essentially a graph with time along the x axis and animations
 * along the y axis.
 * The time is represented with a graduation header at the top and a current
 * time play head.
 * Animations are organized by lines, with a left margin containing the preview
 * of the target DOM element the animation applies to.
 * The current time play head can be moved by clicking/dragging in the header.
 * when this happens, the component emits "current-time-changed" events with the
 * new time.
 *
 * @param {InspectorPanel} inspector.
 */
function AnimationsTimeline(inspector) {
  this.animations = [];
  this.targetNodes = [];
  this.inspector = inspector;

  this.onAnimationStateChanged = this.onAnimationStateChanged.bind(this);
  this.onTimeHeaderMouseDown = this.onTimeHeaderMouseDown.bind(this);
  this.onTimeHeaderMouseUp = this.onTimeHeaderMouseUp.bind(this);
  this.onTimeHeaderMouseOut = this.onTimeHeaderMouseOut.bind(this);
  this.onTimeHeaderMouseMove = this.onTimeHeaderMouseMove.bind(this);

  EventEmitter.decorate(this);
}

exports.AnimationsTimeline = AnimationsTimeline;

AnimationsTimeline.prototype = {
  init: function(containerEl) {
    this.win = containerEl.ownerDocument.defaultView;

    this.rootWrapperEl = createNode({
      parent: containerEl,
      attributes: {
        "class": "animation-timeline"
      }
    });

    this.scrubberEl = createNode({
      parent: this.rootWrapperEl,
      attributes: {
        "class": "scrubber"
      }
    });

    this.timeHeaderEl = createNode({
      parent: this.rootWrapperEl,
      attributes: {
        "class": "time-header"
      }
    });
    this.timeHeaderEl.addEventListener("mousedown", this.onTimeHeaderMouseDown);

    this.animationsEl = createNode({
      parent: this.rootWrapperEl,
      nodeType: "ul",
      attributes: {
        "class": "animations"
      }
    });
  },

  destroy: function() {
    this.stopAnimatingScrubber();
    this.unrender();

    this.timeHeaderEl.removeEventListener("mousedown",
      this.onTimeHeaderMouseDown);

    this.rootWrapperEl.remove();
    this.animations = [];

    this.rootWrapperEl = null;
    this.timeHeaderEl = null;
    this.animationsEl = null;
    this.scrubberEl = null;
    this.win = null;
    this.inspector = null;
  },

  destroyTargetNodes: function() {
    for (let targetNode of this.targetNodes) {
      targetNode.destroy();
    }
    this.targetNodes = [];
  },

  unrender: function() {
    for (let animation of this.animations) {
      animation.off("changed", this.onAnimationStateChanged);
    }

    TimeScale.reset();
    this.destroyTargetNodes();
    this.animationsEl.innerHTML = "";
  },

  onTimeHeaderMouseDown: function(e) {
    this.moveScrubberTo(e.pageX);
    this.win.addEventListener("mouseup", this.onTimeHeaderMouseUp);
    this.win.addEventListener("mouseout", this.onTimeHeaderMouseOut);
    this.win.addEventListener("mousemove", this.onTimeHeaderMouseMove);
  },

  onTimeHeaderMouseUp: function() {
    this.cancelTimeHeaderDragging();
  },

  onTimeHeaderMouseOut: function(e) {
    // Check that mouseout happened on the window itself, and if yes, cancel
    // the dragging.
    if (!this.win.document.contains(e.relatedTarget)) {
      this.cancelTimeHeaderDragging();
    }
  },

  cancelTimeHeaderDragging: function() {
    this.win.removeEventListener("mouseup", this.onTimeHeaderMouseUp);
    this.win.removeEventListener("mouseout", this.onTimeHeaderMouseOut);
    this.win.removeEventListener("mousemove", this.onTimeHeaderMouseMove);
  },

  onTimeHeaderMouseMove: function(e) {
    this.moveScrubberTo(e.pageX);
  },

  moveScrubberTo: function(pageX) {
    this.stopAnimatingScrubber();

    let offset = pageX - this.scrubberEl.offsetWidth;
    if (offset < 0) {
      offset = 0;
    }

    this.scrubberEl.style.left = offset + "px";

    let time = TimeScale.distanceToRelativeTime(offset,
      this.timeHeaderEl.offsetWidth);
    this.emit("current-time-changed", time);
  },

  render: function(animations, documentCurrentTime) {
    this.unrender();

    this.animations = animations;
    if (!this.animations.length) {
      return;
    }

    // Loop first to set the time scale for all current animations.
    for (let {state} of animations) {
      TimeScale.addAnimation(state);
    }

    this.drawHeaderAndBackground();

    for (let animation of this.animations) {
      animation.on("changed", this.onAnimationStateChanged);

      // Each line contains the target animated node and the animation time
      // block.
      let animationEl = createNode({
        parent: this.animationsEl,
        nodeType: "li",
        attributes: {
          "class": "animation"
        }
      });

      // Left sidebar for the animated node.
      let animatedNodeEl = createNode({
        parent: animationEl,
        attributes: {
          "class": "target"
        }
      });

      let timeBlockEl = createNode({
        parent: animationEl,
        attributes: {
          "class": "time-block"
        }
      });

      this.drawTimeBlock(animation, timeBlockEl);

      // Draw the animated node target.
      let targetNode = new AnimationTargetNode(this.inspector, {compact: true});
      targetNode.init(animatedNodeEl);
      targetNode.render(animation);

      // Save the targetNode so it can be destroyed later.
      this.targetNodes.push(targetNode);
    }

    // Use the document's current time to position the scrubber (if the server
    // doesn't provide it, hide the scrubber entirely).
    // Note that because the currentTime was sent via the protocol, some time
    // may have gone by since then, and so the scrubber might be a bit late.
    if (!documentCurrentTime) {
      this.scrubberEl.style.display = "none";
    } else {
      this.scrubberEl.style.display = "block";
      this.startAnimatingScrubber(documentCurrentTime);
    }
  },

  startAnimatingScrubber: function(time) {
    let x = TimeScale.startTimeToDistance(time, this.timeHeaderEl.offsetWidth);
    this.scrubberEl.style.left = x + "px";

    if (time < TimeScale.minStartTime ||
        time > TimeScale.maxEndTime) {
      return;
    }

    let now = this.win.performance.now();
    this.rafID = this.win.requestAnimationFrame(() => {
      this.startAnimatingScrubber(time + this.win.performance.now() - now);
    });
  },

  stopAnimatingScrubber: function() {
    if (this.rafID) {
      this.win.cancelAnimationFrame(this.rafID);
    }
  },

  onAnimationStateChanged: function() {
    // For now, simply re-render the component. The animation front's state has
    // already been updated.
    this.render(this.animations);
  },

  drawHeaderAndBackground: function() {
    let width = this.timeHeaderEl.offsetWidth;
    let scale = width / (TimeScale.maxEndTime - TimeScale.minStartTime);
    drawGraphElementBackground(this.win.document, "time-graduations",
                               width, scale);

    // And the time graduation header.
    this.timeHeaderEl.innerHTML = "";
    let interval = findOptimalTimeInterval(scale, TIME_GRADUATION_MIN_SPACING);
    for (let i = 0; i < width; i += interval) {
      createNode({
        parent: this.timeHeaderEl,
        nodeType: "span",
        attributes: {
          "class": "time-tick",
          "style": `left:${i}px`
        },
        textContent: TimeScale.formatTime(
          TimeScale.distanceToRelativeTime(i, width))
      });
    }
  },

  getAnimationTooltipText: function(state) {
    let getTime = time => L10N.getFormatStr("player.timeLabel",
                            L10N.numberWithDecimals(time / 1000, 2));

    let title = L10N.getFormatStr("timeline." + state.type + ".nameLabel",
                                  state.name);
    let delay = L10N.getStr("player.animationDelayLabel") + " " +
                getTime(state.delay);
    let duration = L10N.getStr("player.animationDurationLabel") + " " +
                   getTime(state.duration);
    let iterations = L10N.getStr("player.animationIterationCountLabel") + " " +
                     (state.iterationCount ||
                      L10N.getStr("player.infiniteIterationCountText"));

    return [title, duration, iterations, delay].join("\n");
  },

  drawTimeBlock: function({state}, el) {
    let width = el.offsetWidth;

    // Create a container element to hold the delay and iterations.
    // It is positioned according to its delay (divided by the playbackrate),
    // and its width is according to its duration (divided by the playbackrate).
    let start = state.startTime;
    let duration = state.duration;
    let rate = state.playbackRate;
    let count = state.iterationCount;
    let delay = state.delay || 0;

    let x = TimeScale.startTimeToDistance(start + (delay / rate), width);
    let w = TimeScale.durationToDistance(duration / rate, width);

    let iterations = createNode({
      parent: el,
      attributes: {
        "class": state.type + " iterations" + (count ? "" : " infinite"),
        // Individual iterations are represented by setting the size of the
        // repeating linear-gradient.
        "style": `left:${x}px;
                  width:${w * (count || 1)}px;
                  background-size:${Math.max(w, 2)}px 100%;`
      }
    });

    // The animation name is displayed over the iterations.
    // Note that in case of negative delay, we push the name towards the right
    // so the delay can be shown.
    createNode({
      parent: iterations,
      attributes: {
        "class": "name",
        "title": this.getAnimationTooltipText(state),
        "style": delay < 0
                 ? "margin-left:" +
                   TimeScale.durationToDistance(Math.abs(delay), width) + "px"
                 : ""
      },
      textContent: state.name
    });

    // Delay.
    if (delay) {
      // Negative delays need to start at 0.
      let x = TimeScale.durationToDistance((delay < 0 ? 0 : delay) / rate, width);
      let w = TimeScale.durationToDistance(Math.abs(delay) / rate, width);
      createNode({
        parent: iterations,
        attributes: {
          "class": "delay",
          "style": `left:-${x}px;
                    width:${w}px;`
        }
      });
    }
  }
};
