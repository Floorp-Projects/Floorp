"use strict";

const {Cu} = require("chrome");
Cu.import("resource://devtools/client/shared/widgets/ViewHelpers.jsm");
const {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
const {DomNodePreview} = require(
  "devtools/client/inspector/shared/dom-node-preview");

// Map dom node fronts by animation fronts so we don't have to get them from the
// walker every time the timeline is refreshed.
var nodeFronts = new WeakMap();

/**
 * UI component responsible for displaying a preview of the target dom node of
 * a given animation.
 * Accepts the same parameters as the DomNodePreview component. See
 * devtools/client/inspector/shared/dom-node-preview.js for documentation.
 */
function AnimationTargetNode(inspector, options) {
  this.inspector = inspector;
  this.previewer = new DomNodePreview(inspector, options);
  EventEmitter.decorate(this);
}

exports.AnimationTargetNode = AnimationTargetNode;

AnimationTargetNode.prototype = {
  init: function(containerEl) {
    this.previewer.init(containerEl);
    this.isDestroyed = false;
  },

  destroy: function() {
    this.previewer.destroy();
    this.inspector = null;
    this.isDestroyed = true;
  },

  render: Task.async(function*(playerFront) {
    // Get the nodeFront from the cache if it was stored previously.
    let nodeFront = nodeFronts.get(playerFront);

    // Try and get it from the playerFront directly next.
    if (!nodeFront) {
      nodeFront = playerFront.animationTargetNodeFront;
    }

    // Finally, get it from the walkerActor if it wasn't found.
    if (!nodeFront) {
      try {
        nodeFront = yield this.inspector.walker.getNodeFromActor(
                               playerFront.actorID, ["node"]);
      } catch (e) {
        // If an error occured while getting the nodeFront and if it can't be
        // attributed to the panel having been destroyed in the meantime, this
        // error needs to be logged and render needs to stop.
        if (!this.isDestroyed) {
          console.error(e);
        }
        return;
      }

      // In all cases, if by now the panel doesn't exist anymore, we need to
      // stop rendering too.
      if (this.isDestroyed) {
        return;
      }
    }

    // Add the nodeFront to the cache.
    nodeFronts.set(playerFront, nodeFront);

    this.previewer.render(nodeFront);
    this.emit("target-retrieved");
  })
};
