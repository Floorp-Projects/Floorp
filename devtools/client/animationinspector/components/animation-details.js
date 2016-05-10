/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu} = require("chrome");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const EventEmitter = require("devtools/shared/event-emitter");
const {createNode, TimeScale} = require("devtools/client/animationinspector/utils");
const {Keyframes} = require("devtools/client/animationinspector/components/keyframes");

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

  this.onFrameSelected = this.onFrameSelected.bind(this);

  this.keyframeComponents = [];
  this.serverTraits = serverTraits;
}

exports.AnimationDetails = AnimationDetails;

AnimationDetails.prototype = {
  // These are part of frame objects but are not animated properties. This
  // array is used to skip them.
  NON_PROPERTIES: ["easing", "composite", "computedOffset", "offset"],

  init: function (containerEl) {
    this.containerEl = containerEl;
  },

  destroy: function () {
    this.unrender();
    this.containerEl = null;
    this.serverTraits = null;
  },

  unrender: function () {
    for (let component of this.keyframeComponents) {
      component.off("frame-selected", this.onFrameSelected);
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

        for (let {value, offset} of values) {
          tracks[name].push({value, offset});
        }
      }
    } else {
      let frames = yield this.animation.getFrames();
      for (let frame of frames) {
        for (let name in frame) {
          if (this.NON_PROPERTIES.indexOf(name) != -1) {
            continue;
          }

          if (!tracks[name]) {
            tracks[name] = [];
          }

          tracks[name].push({
            value: frame[name],
            offset: frame.computedOffset
          });
        }
      }
    }

    return tracks;
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

    // Useful for tests to know when the keyframes have been retrieved.
    this.emit("keyframes-retrieved");

    for (let propertyName in this.tracks) {
      let line = createNode({
        parent: this.containerEl,
        attributes: {"class": "property"}
      });
      let {warning, className} =
        this.getPerfDataForProperty(animation, propertyName);
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

      // Scale the list of keyframes according to the current time scale.
      let {x, w} = TimeScale.getAnimationDimensions(animation);
      framesEl.style.left = `${x}%`;
      framesEl.style.width = `${w}%`;

      let keyframesComponent = new Keyframes();
      keyframesComponent.init(framesEl);
      keyframesComponent.render({
        keyframes: this.tracks[propertyName],
        propertyName: propertyName,
        animation: animation
      });
      keyframesComponent.on("frame-selected", this.onFrameSelected);

      this.keyframeComponents.push(keyframesComponent);
    }
  }),

  onFrameSelected: function (e, args) {
    // Relay the event up, it's needed in parents too.
    this.emit(e, args);
  }
};

/**
 * Turn propertyName into property-name.
 * @param {String} jsPropertyName A camelcased CSS property name. Typically
 * something that comes out of computed styles. E.g. borderBottomColor
 * @return {String} The corresponding CSS property name: border-bottom-color
 */
function getCssPropertyName(jsPropertyName) {
  return jsPropertyName.replace(/[A-Z]/g, "-$&").toLowerCase();
}
exports.getCssPropertyName = getCssPropertyName;
