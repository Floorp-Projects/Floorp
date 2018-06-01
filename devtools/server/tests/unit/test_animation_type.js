/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test the output of AnimationPlayerActor.getType().

const { ANIMATION_TYPES, AnimationPlayerActor } =
  require("devtools/server/actors/animation");

function run_test() {
  // Mock a window with just the properties the AnimationPlayerActor uses.
  const window = {
    MutationObserver: function() {
      this.observe = () => {};
    },
    Animation: function() {
      this.effect = {target: getMockNode()};
    },
    CSSAnimation: function() {
      this.effect = {target: getMockNode()};
    },
    CSSTransition: function() {
      this.effect = {target: getMockNode()};
    }
  };

  window.CSSAnimation.prototype = Object.create(window.Animation.prototype);
  window.CSSTransition.prototype = Object.create(window.Animation.prototype);

  // Helper to get a mock DOM node.
  function getMockNode() {
    return {
      ownerDocument: {
        defaultView: window
      }
    };
  }

  // Objects in this array should contain the following properties:
  // - desc {String} For logging
  // - animation {Object} An animation object instantiated from one of the mock
  //   window animation constructors.
  // - expectedType {String} The expected type returned by
  //   AnimationPlayerActor.getType.
  const TEST_DATA = [{
    desc: "Test CSSAnimation type",
    animation: new window.CSSAnimation(),
    expectedType: ANIMATION_TYPES.CSS_ANIMATION
  }, {
    desc: "Test CSSTransition type",
    animation: new window.CSSTransition(),
    expectedType: ANIMATION_TYPES.CSS_TRANSITION
  }, {
    desc: "Test ScriptAnimation type",
    animation: new window.Animation(),
    expectedType: ANIMATION_TYPES.SCRIPT_ANIMATION
  }, {
    desc: "Test unknown type",
    animation: {effect: {target: getMockNode()}},
    expectedType: ANIMATION_TYPES.UNKNOWN
  }];

  for (const { desc, animation, expectedType } of TEST_DATA) {
    info(desc);
    const actor = AnimationPlayerActor({}, animation);
    Assert.equal(actor.getType(), expectedType);
  }
}
