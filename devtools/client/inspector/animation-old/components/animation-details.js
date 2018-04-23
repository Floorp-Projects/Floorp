/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {createNode, getCssPropertyName} =
  require("devtools/client/inspector/animation-old/utils");
const {Keyframes} = require("devtools/client/inspector/animation-old/components/keyframes");

const { LocalizationHelper } = require("devtools/shared/l10n");
const L10N =
  new LocalizationHelper("devtools/client/locales/animationinspector.properties");

/**
 * UI component responsible for displaying detailed information for a given
 * animation.
 * This includes information about timing, easing, keyframes, animated
 * properties.
 *
 * @param {Object} serverTraits The list of server-side capabilities.
 */
function AnimationDetails(serverTraits) {
  this.keyframeComponents = [];
  this.serverTraits = serverTraits;
}

exports.AnimationDetails = AnimationDetails;

AnimationDetails.prototype = {
  // These are part of frame objects but are not animated properties. This
  // array is used to skip them.
  NON_PROPERTIES: ["easing", "composite", "computedOffset",
                   "offset", "simulateComputeValuesFailure"],

  init: function(containerEl) {
    this.containerEl = containerEl;
  },

  destroy: function() {
    this.unrender();
    this.containerEl = null;
    this.serverTraits = null;
    this.progressIndicatorEl = null;
  },

  unrender: function() {
    for (let component of this.keyframeComponents) {
      component.destroy();
    }
    this.keyframeComponents = [];

    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
  },

  getPerfDataForProperty: function(animation, propertyName) {
    let warning = "";
    let className = "";
    if (animation.state.propertyState) {
      let isRunningOnCompositor;
      for (let propState of animation.state.propertyState) {
        if (propState.property == propertyName) {
          isRunningOnCompositor = propState.runningOnCompositor;
          if (typeof propState.warning != "undefined") {
            warning = propState.warning;
          }
          break;
        }
      }
      if (isRunningOnCompositor && warning == "") {
        className = "oncompositor";
      } else if (!isRunningOnCompositor && warning != "") {
        className = "warning";
      }
    }
    return {className, warning};
  },

  /**
   * Get animation types of given CSS property names.
   * @param {Array} CSS property names.
   *                e.g. ["background-color", "opacity", ...]
   * @return {Object} Animation type mapped with CSS property name.
   *                  e.g. { "background-color": "color", }
   *                         "opacity": "float", ... }
   */
  async getAnimationTypes(propertyNames) {
    if (this.serverTraits.hasGetAnimationTypes) {
      return this.animation.getAnimationTypes(propertyNames);
    }
    // Set animation type 'none' since does not support getAnimationTypes.
    const animationTypes = {};
    propertyNames.forEach(propertyName => {
      animationTypes[propertyName] = "none";
    });
    return Promise.resolve(animationTypes);
  },

  async render(animation, tracks) {
    this.unrender();

    if (!animation) {
      return;
    }
    this.animation = animation;
    this.tracks = tracks;

    // We might have been destroyed in the meantime, or the component might
    // have been re-rendered.
    if (!this.containerEl || this.animation !== animation) {
      return;
    }

    // Get animation type for each CSS properties.
    const animationTypes = await this.getAnimationTypes(Object.keys(this.tracks));

    // Render progress indicator.
    this.renderProgressIndicator();
    // Render animated properties header.
    this.renderAnimatedPropertiesHeader();
    // Render animated properties body.
    this.renderAnimatedPropertiesBody(animationTypes);

    // Create dummy animation to indicate the animation progress.
    const timing = Object.assign({}, animation.state, {
      iterations: animation.state.iterationCount
                  ? animation.state.iterationCount : Infinity
    });
    this.dummyAnimation =
      new this.win.Animation(new this.win.KeyframeEffect(null, null, timing), null);
  },

  renderAnimatedPropertiesHeader: function() {
    // Add animated property header.
    const headerEl = createNode({
      parent: this.containerEl,
      attributes: { "class": "animated-properties-header" }
    });

    // Add progress tick container.
    const progressTickContainerEl = createNode({
      parent: this.containerEl,
      attributes: { "class": "progress-tick-container track-container" }
    });

    // Add label container.
    const headerLabelContainerEl = createNode({
      parent: headerEl,
      attributes: { "class": "track-container" }
    });

    // Add labels
    for (let label of [L10N.getFormatStr("detail.propertiesHeader.percentage", 0),
                       L10N.getFormatStr("detail.propertiesHeader.percentage", 50),
                       L10N.getFormatStr("detail.propertiesHeader.percentage", 100)]) {
      createNode({
        parent: progressTickContainerEl,
        nodeType: "span",
        attributes: { "class": "progress-tick" }
      });
      createNode({
        parent: headerLabelContainerEl,
        nodeType: "label",
        attributes: { "class": "header-item" },
        textContent: label
      });
    }
  },

  renderAnimatedPropertiesBody: function(animationTypes) {
    // Add animated property body.
    const bodyEl = createNode({
      parent: this.containerEl,
      attributes: { "class": "animated-properties-body" }
    });

    // Move unchanged value animation to bottom in the list.
    const propertyNames = [];
    const unchangedPropertyNames = [];
    for (let propertyName in this.tracks) {
      if (!isUnchangedProperty(this.tracks[propertyName])) {
        propertyNames.push(propertyName);
      } else {
        unchangedPropertyNames.push(propertyName);
      }
    }
    Array.prototype.push.apply(propertyNames, unchangedPropertyNames);

    for (let propertyName of propertyNames) {
      let line = createNode({
        parent: bodyEl,
        attributes: {"class": "property"}
      });
      if (unchangedPropertyNames.includes(propertyName)) {
        line.classList.add("unchanged");
      }
      let {warning, className} =
        this.getPerfDataForProperty(this.animation, propertyName);
      createNode({
        // text-overflow doesn't work in flex items, so we need a second level
        // of container to actually have an ellipsis on the name.
        // See bug 972664.
        parent: createNode({
          parent: line,
          attributes: {"class": "name"}
        }),
        textContent: getCssPropertyName(propertyName),
        attributes: {"title": warning,
                     "class": className}
      });

      // Add the keyframes diagram for this property.
      let framesWrapperEl = createNode({
        parent: line,
        attributes: {"class": "track-container"}
      });

      let framesEl = createNode({
        parent: framesWrapperEl,
        attributes: {"class": "frames"}
      });

      let keyframesComponent = new Keyframes();
      keyframesComponent.init(framesEl);
      keyframesComponent.render({
        keyframes: this.tracks[propertyName],
        propertyName: propertyName,
        animation: this.animation,
        animationType: animationTypes[propertyName]
      });
      this.keyframeComponents.push(keyframesComponent);
    }
  },

  renderProgressIndicator: function() {
    // The wrapper represents the area which the indicator is displayable.
    const progressIndicatorWrapperEl = createNode({
      parent: this.containerEl,
      attributes: {
        "class": "track-container progress-indicator-wrapper"
      }
    });
    this.progressIndicatorEl = createNode({
      parent: progressIndicatorWrapperEl,
      attributes: {
        "class": "progress-indicator"
      }
    });
    createNode({
      parent: this.progressIndicatorEl,
      attributes: {
        "class": "progress-indicator-shape"
      }
    });
  },

  indicateProgress: function(time) {
    if (!this.progressIndicatorEl) {
      // Not displayed yet.
      return;
    }
    const startTime = this.animation.state.previousStartTime || 0;
    this.dummyAnimation.currentTime =
      (time - startTime) * this.animation.state.playbackRate;
    this.progressIndicatorEl.style.left =
      `${ this.dummyAnimation.effect.getComputedTiming().progress * 100 }%`;
  },

  get win() {
    return this.containerEl.ownerDocument.defaultView;
  }
};

function isUnchangedProperty(values) {
  const firstValue = values[0].value;
  for (let i = 1; i < values.length; i++) {
    if (values[i].value !== firstValue) {
      return false;
    }
  }
  return true;
}
