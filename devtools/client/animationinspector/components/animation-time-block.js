/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {createNode, TimeScale} = require("devtools/client/animationinspector/utils");

const { LocalizationHelper } = require("devtools/client/shared/l10n");

const STRINGS_URI = "chrome://devtools/locale/animationinspector.properties";
const L10N = new LocalizationHelper(STRINGS_URI);

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

    // background properties for .iterations element
    let backgroundIterations = TimeScale.getIterationsBackgroundData(animation);

    createNode({
      parent: this.containerEl,
      attributes: {
        "class": "iterations" + (state.iterationCount ? "" : " infinite"),
        // Individual iterations are represented by setting the size of the
        // repeating linear-gradient.
        // The background-size, background-position, background-repeat represent
        // iterationCount and iterationStart.
        "style": `left:${x}%;
                  width:${iterationW}%;
                  background-size:${backgroundIterations.size}% 100%;
                  background-position:${backgroundIterations.position}% 0;
                  background-repeat:${backgroundIterations.repeat};`
      }
    });

    // The animation name is displayed over the iterations.
    // Note that in case of negative delay, it is pushed towards the right so
    // the delay element does not overlap.
    createNode({
      parent: createNode({
        parent: this.containerEl,
        attributes: {
          "class": "name",
          "title": this.getTooltipText(state),
          // Place the name at the same position as the iterations, but make
          // space for the negative delay if any.
          "style": `left:${x + negativeDelayW}%;
                    width:${iterationW - negativeDelayW}%;`
        },
      }),
      textContent: state.name
    });

    // Delay.
    if (state.delay) {
      // Negative delays need to start at 0.
      createNode({
        parent: this.containerEl,
        attributes: {
          "class": "delay" + (state.delay < 0 ? " negative" : ""),
          "style": `left:${delayX}%;
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

    // Adding the iteration start.
    if (state.iterationStart !== 0) {
      let iterationStartTime = state.iterationStart * state.duration / 1000;
      text += L10N.getFormatStr("player.animationIterationStartLabel",
                                state.iterationStart,
                                L10N.numberWithDecimals(iterationStartTime, 2));
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
 * "some-name", "some-name : CSS Transition", "some-name : CSS Animation",
 * "some-name : Script Animation", or "Script Animation", depending
 * if the server provides the type, what type it is and if the animation
 * has a name
 * @param {AnimationPlayerFront} animation
 */
function getFormattedAnimationTitle({state}) {
  // Older servers don't send a type, and only know about
  // CSSAnimations and CSSTransitions, so it's safe to use
  // just the name.
  if (!state.type) {
    return state.name;
  }

  // Script-generated animations may not have a name.
  if (state.type === "scriptanimation" && !state.name) {
    return L10N.getStr("timeline.scriptanimation.unnamedLabel");
  }

  return L10N.getFormatStr(`timeline.${state.type}.nameLabel`, state.name);
}
