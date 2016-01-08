"use strict";

const {Cu} = require("chrome");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {
  createNode,
  TimeScale
} = require("devtools/client/animationinspector/utils");
const {Keyframes} = require("devtools/client/animationinspector/components/keyframes");
/**
 * UI component responsible for displaying detailed information for a given
 * animation.
 * This includes information about timing, easing, keyframes, animated
 * properties.
 */
function AnimationDetails() {
  EventEmitter.decorate(this);

  this.onFrameSelected = this.onFrameSelected.bind(this);

  this.keyframeComponents = [];
}

exports.AnimationDetails = AnimationDetails;

AnimationDetails.prototype = {
  // These are part of frame objects but are not animated properties. This
  // array is used to skip them.
  NON_PROPERTIES: ["easing", "composite", "computedOffset", "offset"],

  init: function(containerEl) {
    this.containerEl = containerEl;
  },

  destroy: function() {
    this.unrender();
    this.containerEl = null;
  },

  unrender: function() {
    for (let component of this.keyframeComponents) {
      component.off("frame-selected", this.onFrameSelected);
      component.destroy();
    }
    this.keyframeComponents = [];

    while (this.containerEl.firstChild) {
      this.containerEl.firstChild.remove();
    }
  },

  /**
   * Convert a list of frames into a list of tracks, one per animated property,
   * each with a list of frames.
   */
  getTracksFromFrames: function(frames) {
    let tracks = {};

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

    return tracks;
  },

  render: Task.async(function*(animation) {
    this.unrender();

    if (!animation) {
      return;
    }
    this.animation = animation;

    let frames = yield animation.getFrames();

    // We might have been destroyed in the meantime, or the component might
    // have been re-rendered.
    if (!this.containerEl || this.animation !== animation) {
      return;
    }
    // Useful for tests to know when the keyframes have been retrieved.
    this.emit("keyframes-retrieved");

    // Build an element for each animated property track.
    this.tracks = this.getTracksFromFrames(frames);
    for (let propertyName in this.tracks) {
      let line = createNode({
        parent: this.containerEl,
        attributes: {"class": "property"}
      });

      createNode({
        // text-overflow doesn't work in flex items, so we need a second level
        // of container to actually have an ellipsis on the name.
        // See bug 972664.
        parent: createNode({
          parent: line,
          attributes: {"class": "name"},
        }),
        textContent: getCssPropertyName(propertyName)
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

  onFrameSelected: function(e, args) {
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
