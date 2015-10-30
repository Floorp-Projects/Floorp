/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {
  createNode,
  drawGraphElementBackground,
  findOptimalTimeInterval,
  TargetNodeHighlighter
} = require("devtools/client/animationinspector/utils");

const STRINGS_URI = "chrome://browser/locale/devtools/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);
const MILLIS_TIME_FORMAT_MAX_DURATION = 4000;
// The minimum spacing between 2 time graduation headers in the timeline (px).
const TIME_GRADUATION_MIN_SPACING = 40;

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
  this.onHighlightNodeClick = this.onHighlightNodeClick.bind(this);
  this.onTargetHighlighterLocked = this.onTargetHighlighterLocked.bind(this);

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
    this.highlightNodeEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "class": "node-highlighter",
        "title": L10N.getStr("node.highlightNodeLabel")
      }
    });

    // Wrapper used for mouseover/out event handling.
    this.previewEl = createNode({
      parent: this.el,
      nodeType: "span",
      attributes: {
        "title": L10N.getStr("node.selectNodeLabel")
      }
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
    this.previewEl.addEventListener("click", this.onSelectNodeClick);
    this.highlightNodeEl.addEventListener("click", this.onHighlightNodeClick);

    // Start to listen for markupmutation events.
    this.inspector.on("markupmutation", this.onMarkupMutations);

    // Listen to the target node highlighter.
    TargetNodeHighlighter.on("highlighted", this.onTargetHighlighterLocked);
  },

  destroy: function() {
    TargetNodeHighlighter.unhighlight().catch(e => console.error(e));

    TargetNodeHighlighter.off("highlighted", this.onTargetHighlighterLocked);
    this.inspector.off("markupmutation", this.onMarkupMutations);
    this.previewEl.removeEventListener("mouseover", this.onPreviewMouseOver);
    this.previewEl.removeEventListener("mouseout", this.onPreviewMouseOut);
    this.previewEl.removeEventListener("click", this.onSelectNodeClick);
    this.highlightNodeEl.removeEventListener("click", this.onHighlightNodeClick);

    this.el.remove();
    this.el = this.tagNameEl = this.idEl = this.classEl = null;
    this.highlightNodeEl = this.previewEl = null;
    this.nodeFront = this.inspector = this.playerFront = null;
  },

  get highlighterUtils() {
    return this.inspector.toolbox.highlighterUtils;
  },

  onPreviewMouseOver: function() {
    if (!this.nodeFront) {
      return;
    }
    this.highlighterUtils.highlightNodeFront(this.nodeFront);
  },

  onPreviewMouseOut: function() {
    if (!this.nodeFront) {
      return;
    }
    this.highlighterUtils.unhighlight();
  },

  onSelectNodeClick: function() {
    if (!this.nodeFront) {
      return;
    }
    this.inspector.selection.setNodeFront(this.nodeFront, "animationinspector");
  },

  onHighlightNodeClick: function() {
    let classList = this.highlightNodeEl.classList;

    let isHighlighted = classList.contains("selected");
    if (isHighlighted) {
      classList.remove("selected");
      TargetNodeHighlighter.unhighlight().then(() => {
        this.emit("target-highlighter-unlocked");
      }, e => console.error(e));
    } else {
      classList.add("selected");
      TargetNodeHighlighter.highlight(this).then(() => {
        this.emit("target-highlighter-locked");
      }, e => console.error(e));
    }
  },

  onTargetHighlighterLocked: function(e, animationTargetNode) {
    if (animationTargetNode !== this) {
      this.highlightNodeEl.classList.remove("selected");
    }
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
      if (!this.el) {
        // The panel was destroyed in the meantime. Just log a warning.
        console.warn("Cound't retrieve the animation target node, widget " +
                     "destroyed");
      } else {
        // This was an unexpected error, log it.
        console.error(e);
      }
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
var TimeScale = {
  minStartTime: Infinity,
  maxEndTime: 0,

  /**
   * Add a new animation to time scale.
   * @param {Object} state A PlayerFront.state object.
   */
  addAnimation: function(state) {
    let {previousStartTime, delay, duration,
         iterationCount, playbackRate} = state;

    // Negative-delayed animations have their startTimes set such that we would
    // be displaying the delay outside the time window if we didn't take it into
    // account here.
    let relevantDelay = delay < 0 ? delay / playbackRate : 0;
    previousStartTime = previousStartTime || 0;

    this.minStartTime = Math.min(this.minStartTime,
                                 previousStartTime + relevantDelay);
    let length = (delay / playbackRate) +
                 ((duration / playbackRate) *
                  (!iterationCount ? 1 : iterationCount));
    this.maxEndTime = Math.max(this.maxEndTime, previousStartTime + length);
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
 * when this happens, the component emits "current-data-changed" events with the
 * new time and state of the timeline.
 *
 * @param {InspectorPanel} inspector.
 */
function AnimationsTimeline(inspector) {
  this.animations = [];
  this.targetNodes = [];
  this.timeBlocks = [];
  this.inspector = inspector;
  this.onAnimationStateChanged = this.onAnimationStateChanged.bind(this);
  this.onScrubberMouseDown = this.onScrubberMouseDown.bind(this);
  this.onScrubberMouseUp = this.onScrubberMouseUp.bind(this);
  this.onScrubberMouseOut = this.onScrubberMouseOut.bind(this);
  this.onScrubberMouseMove = this.onScrubberMouseMove.bind(this);
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

    this.scrubberHandleEl = createNode({
      parent: this.scrubberEl,
      attributes: {
        "class": "scrubber-handle"
      }
    });
    this.scrubberHandleEl.addEventListener("mousedown", this.onScrubberMouseDown);

    this.timeHeaderEl = createNode({
      parent: this.rootWrapperEl,
      attributes: {
        "class": "time-header"
      }
    });
    this.timeHeaderEl.addEventListener("mousedown", this.onScrubberMouseDown);

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
      this.onScrubberMouseDown);
    this.scrubberHandleEl.removeEventListener("mousedown",
      this.onScrubberMouseDown);

    this.rootWrapperEl.remove();
    this.animations = [];

    this.rootWrapperEl = null;
    this.timeHeaderEl = null;
    this.animationsEl = null;
    this.scrubberEl = null;
    this.scrubberHandleEl = null;
    this.win = null;
    this.inspector = null;
  },
  destroyTargetNodes: function() {
    for (let targetNode of this.targetNodes) {
      targetNode.destroy();
    }
    this.targetNodes = [];
  },
  destroyTimeBlocks: function() {
    for (let timeBlock of this.timeBlocks) {
      timeBlock.destroy();
    }
    this.timeBlocks = [];
  },

  unrender: function() {
    for (let animation of this.animations) {
      animation.off("changed", this.onAnimationStateChanged);
    }
    TimeScale.reset();
    this.destroyTargetNodes();
    this.destroyTimeBlocks();
    this.animationsEl.innerHTML = "";
  },

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

  moveScrubberTo: function(pageX) {
    this.stopAnimatingScrubber();

    let offset = pageX - this.scrubberEl.offsetWidth;
    if (offset < 0) {
      offset = 0;
    }

    this.scrubberEl.style.left = offset + "px";

    let time = TimeScale.distanceToRelativeTime(offset,
      this.timeHeaderEl.offsetWidth);

    this.emit("timeline-data-changed", {
      isPaused: true,
      isMoving: false,
      isUserDrag: true,
      time: time
    });
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
      // Draw the animated node target.
      let targetNode = new AnimationTargetNode(this.inspector, {compact: true});
      targetNode.init(animatedNodeEl);
      targetNode.render(animation);
      this.targetNodes.push(targetNode);

      // Right-hand part contains the timeline itself (called time-block here).
      let timeBlockEl = createNode({
        parent: animationEl,
        attributes: {
          "class": "time-block"
        }
      });
      // Draw the animation time block.
      let timeBlock = new AnimationTimeBlock();
      timeBlock.init(timeBlockEl);
      timeBlock.render(animation);
      this.timeBlocks.push(timeBlock);
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
    let x = TimeScale.startTimeToDistance(time, this.timeHeaderEl.offsetWidth);
    this.scrubberEl.style.left = x + "px";

    // Only stop the scrubber if it's out of bounds or all animations have been
    // paused, but not if at least an animation is infinite.
    let isOutOfBounds = time < TimeScale.minStartTime ||
                        time > TimeScale.maxEndTime;
    let isAllPaused = !this.isAtLeastOneAnimationPlaying();
    let hasInfinite = this.hasInfiniteAnimations();

    if (isAllPaused || (isOutOfBounds && !hasInfinite)) {
      this.stopAnimatingScrubber();
      this.emit("timeline-data-changed", {
        isPaused: !this.isAtLeastOneAnimationPlaying(),
        isMoving: false,
        isUserDrag: false,
        time: TimeScale.distanceToRelativeTime(x, this.timeHeaderEl.offsetWidth)
      });
      return;
    }

    this.emit("timeline-data-changed", {
      isPaused: false,
      isMoving: true,
      isUserDrag: false,
      time: TimeScale.distanceToRelativeTime(x, this.timeHeaderEl.offsetWidth)
    });

    let now = this.win.performance.now();
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
  }
};

/**
 * UI component responsible for displaying a single animation timeline, which
 * basically looks like a rectangle that shows the delay and iterations.
 */
function AnimationTimeBlock() {}

exports.AnimationTimeBlock = AnimationTimeBlock;

AnimationTimeBlock.prototype = {
  init: function(containerEl) {
    this.containerEl = containerEl;
  },
  destroy: function() {
    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
    this.containerEl = null;
    this.animation = null;
  },

  render: function(animation) {
    this.animation = animation;
    let {state} = this.animation;

    let width = this.containerEl.offsetWidth;

    // Create a container element to hold the delay and iterations.
    // It is positioned according to its delay (divided by the playbackrate),
    // and its width is according to its duration (divided by the playbackrate).
    let start = state.previousStartTime || 0;
    let duration = state.duration;
    let rate = state.playbackRate;
    let count = state.iterationCount;
    let delay = state.delay || 0;

    let x = TimeScale.startTimeToDistance(start + (delay / rate), width);
    let w = TimeScale.durationToDistance(duration / rate, width);

    let iterations = createNode({
      parent: this.containerEl,
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
        "title": this.getTooltipText(state),
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
      let delayX = TimeScale.durationToDistance(
        (delay < 0 ? 0 : delay) / rate, width);
      let delayW = TimeScale.durationToDistance(
        Math.abs(delay) / rate, width);

      createNode({
        parent: iterations,
        attributes: {
          "class": "delay" + (delay < 0 ? " negative" : ""),
          "style": `left:-${delayX}px;
                    width:${delayW}px;`
        }
      });
    }
  },

  getTooltipText: function(state) {
    let getTime = time => L10N.getFormatStr("player.timeLabel",
                            L10N.numberWithDecimals(time / 1000, 2));
    // The type isn't always available, older servers don't send it.
    let title =
      state.type
      ? L10N.getFormatStr("timeline." + state.type + ".nameLabel", state.name)
      : state.name;
    let delay = L10N.getStr("player.animationDelayLabel") + " " +
                getTime(state.delay);
    let duration = L10N.getStr("player.animationDurationLabel") + " " +
                   getTime(state.duration);
    let iterations = L10N.getStr("player.animationIterationCountLabel") + " " +
                     (state.iterationCount ||
                      L10N.getStr("player.infiniteIterationCountText"));
    return [title, duration, iterations, delay].join("\n");
  }
};
