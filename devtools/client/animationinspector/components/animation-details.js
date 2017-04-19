/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Task} = require("devtools/shared/task");
const EventEmitter = require("devtools/shared/event-emitter");
const {createNode, getCssPropertyName} =
  require("devtools/client/animationinspector/utils");
const {Keyframes} = require("devtools/client/animationinspector/components/keyframes");

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
  EventEmitter.decorate(this);

  this.keyframeComponents = [];
  this.serverTraits = serverTraits;
}

exports.AnimationDetails = AnimationDetails;

AnimationDetails.prototype = {
  // These are part of frame objects but are not animated properties. This
  // array is used to skip them.
  NON_PROPERTIES: ["easing", "composite", "computedOffset",
                   "offset", "simulateComputeValuesFailure"],

  init: function (containerEl) {
    this.containerEl = containerEl;
  },

  destroy: function () {
    this.unrender();
    this.containerEl = null;
    this.serverTraits = null;
    this.progressIndicatorEl = null;
  },

  unrender: function () {
    for (let component of this.keyframeComponents) {
      component.destroy();
    }
    this.keyframeComponents = [];

    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
  },

  getPerfDataForProperty: function (animation, propertyName) {
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
   * Get a list of the tracks of the animation actor
   * @return {Object} A list of tracks, one per animated property, each
   * with a list of keyframes
   */
  getTracks: Task.async(function* () {
    let tracks = {};

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
      let properties = yield this.animation.getProperties();
      for (let {name, values} of properties) {
        if (!tracks[name]) {
          tracks[name] = [];
        }

        for (let {value, offset, easing, distance} of values) {
          distance = distance ? distance : 0;
          tracks[name].push({value, offset, easing, distance});
        }
      }
    } else {
      let frames = yield this.animation.getFrames();
      for (let frame of frames) {
        for (let name in frame) {
          if (this.NON_PROPERTIES.indexOf(name) != -1) {
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
            offset: frame.computedOffset,
            easing: frame.easing,
            distance: 0
          });
        }
      }
    }

    return tracks;
  }),

  /**
   * Get animation types of given CSS property names.
   * @param {Array} CSS property names.
   *                e.g. ["background-color", "opacity", ...]
   * @return {Object} Animation type mapped with CSS property name.
   *                  e.g. { "background-color": "color", }
   *                         "opacity": "float", ... }
   */
  getAnimationTypes: Task.async(function* (propertyNames) {
    if (this.serverTraits.hasGetAnimationTypes) {
      return yield this.animation.getAnimationTypes(propertyNames);
    }
    // Set animation type 'none' since does not support getAnimationTypes.
    const animationTypes = {};
    propertyNames.forEach(propertyName => {
      animationTypes[propertyName] = "none";
    });
    return Promise.resolve(animationTypes);
  }),

  render: Task.async(function* (animation) {
    this.unrender();

    if (!animation) {
      return;
    }
    this.animation = animation;

    // We might have been destroyed in the meantime, or the component might
    // have been re-rendered.
    if (!this.containerEl || this.animation !== animation) {
      return;
    }

    // Build an element for each animated property track.
    this.tracks = yield this.getTracks(animation, this.serverTraits);

    // Get animation type for each CSS properties.
    const animationTypes = yield this.getAnimationTypes(Object.keys(this.tracks));

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

    // Useful for tests to know when rendering of all animation detail UIs
    // have been completed.
    this.emit("animation-detail-rendering-completed");
  }),

  renderAnimatedPropertiesHeader: function () {
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

  renderAnimatedPropertiesBody: function (animationTypes) {
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

  renderProgressIndicator: function () {
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

  indicateProgress: function (time) {
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
