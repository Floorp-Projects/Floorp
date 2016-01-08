"use strict";

const {Cu} = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {
  createNode,
  TimeScale
} = require("devtools/client/animationinspector/utils");

const STRINGS_URI = "chrome://devtools/locale/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

/**
 * UI component responsible for displaying a single animation timeline, which
 * basically looks like a rectangle that shows the delay and iterations.
 */
function AnimationTimeBlock() {
  EventEmitter.decorate(this);
  this.onClick = this.onClick.bind(this);
}

exports.AnimationTimeBlock = AnimationTimeBlock;

AnimationTimeBlock.prototype = {
  init: function(containerEl) {
    this.containerEl = containerEl;
    this.containerEl.addEventListener("click", this.onClick);
  },

  destroy: function() {
    this.containerEl.removeEventListener("click", this.onClick);
    this.unrender();
    this.containerEl = null;
    this.animation = null;
  },

  unrender: function() {
    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
  },

  render: function(animation) {
    this.unrender();

    this.animation = animation;
    let {state} = this.animation;

    // Create a container element to hold the delay and iterations.
    // It is positioned according to its delay (divided by the playbackrate),
    // and its width is according to its duration (divided by the playbackrate).
    let {x, iterationW, delayX, delayW, negativeDelayW} =
      TimeScale.getAnimationDimensions(animation);

    let iterations = createNode({
      parent: this.containerEl,
      attributes: {
        "class": state.type + " iterations" +
                 (state.iterationCount ? "" : " infinite"),
        // Individual iterations are represented by setting the size of the
        // repeating linear-gradient.
        "style": `left:${x}%;
                  width:${iterationW}%;
                  background-size:${100 / (state.iterationCount || 1)}% 100%;`
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
        // Make space for the negative delay with a margin-left.
        "style": `margin-left:${negativeDelayW}%`
      },
      textContent: state.name
    });

    // Delay.
    if (state.delay) {
      // Negative delays need to start at 0.
      createNode({
        parent: iterations,
        attributes: {
          "class": "delay" + (state.delay < 0 ? " negative" : ""),
          "style": `left:-${delayX}%;
                    width:${delayW}%;`
        }
      });
    }
  },

  getTooltipText: function(state) {
    let getTime = time => L10N.getFormatStr("player.timeLabel",
                            L10N.numberWithDecimals(time / 1000, 2));

    let text = "";

    // Adding the name.
    text += getFormattedAnimationTitle({state});
    text += "\n";

    // Adding the delay.
    text += L10N.getStr("player.animationDelayLabel") + " ";
    text += getTime(state.delay);
    text += "\n";

    // Adding the duration.
    text += L10N.getStr("player.animationDurationLabel") + " ";
    text += getTime(state.duration);
    text += "\n";

    // Adding the iteration count (the infinite symbol, or an integer).
    if (state.iterationCount !== 1) {
      text += L10N.getStr("player.animationIterationCountLabel") + " ";
      text += state.iterationCount ||
              L10N.getStr("player.infiniteIterationCountText");
      text += "\n";
    }

    // Adding the playback rate if it's different than 1.
    if (state.playbackRate !== 1) {
      text += L10N.getStr("player.animationRateLabel") + " ";
      text += state.playbackRate;
      text += "\n";
    }

    // Adding a note that the animation is running on the compositor thread if
    // needed.
    if (state.isRunningOnCompositor) {
      text += L10N.getStr("player.runningOnCompositorTooltip");
    }

    return text;
  },

  onClick: function(e) {
    e.stopPropagation();
    this.emit("selected", this.animation);
  }
};

/**
 * Get a formatted title for this animation. This will be either:
 * "some-name", "some-name : CSS Transition", or "some-name : CSS Animation",
 * depending if the server provides the type, and what type it is.
 * @param {AnimationPlayerFront} animation
 */
function getFormattedAnimationTitle({state}) {
  // Older servers don't send the type.
  return state.type
    ? L10N.getFormatStr("timeline." + state.type + ".nameLabel", state.name)
    : state.name;
}
