/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that AnimationPlayerActor.getName returns the right name depending on
// the type of an animation and the various properties available on it.

const { AnimationPlayerActor } = require("devtools/server/actors/animation");

function run_test() {
  // Mock a window with just the properties the AnimationPlayerActor uses.
  let window = {
    MutationObserver: function () {
      this.observe = () => {};
    },
    Animation: function () {
      this.effect = {target: getMockNode()};
    },
    CSSAnimation: function () {
      this.effect = {target: getMockNode()};
    },
    CSSTransition: function () {
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
  // - props {Objet} Properties of this object will be added to the animation
  //   object.
  // - expectedName {String} The expected name returned by
  //   AnimationPlayerActor.getName.
  const TEST_DATA = [{
    desc: "Animation with an id",
    animation: new window.Animation(),
    props: { id: "animation-id" },
    expectedName: "animation-id"
  }, {
    desc: "Animation without an id",
    animation: new window.Animation(),
    props: {},
    expectedName: ""
  }, {
    desc: "CSSTransition with an id",
    animation: new window.CSSTransition(),
    props: { id: "transition-with-id", transitionProperty: "width" },
    expectedName: "transition-with-id"
  }, {
    desc: "CSSAnimation with an id",
    animation: new window.CSSAnimation(),
    props: { id: "animation-with-id", animationName: "move" },
    expectedName: "animation-with-id"
  }, {
    desc: "CSSTransition without an id",
    animation: new window.CSSTransition(),
    props: { transitionProperty: "width" },
    expectedName: "width"
  }, {
    desc: "CSSAnimation without an id",
    animation: new window.CSSAnimation(),
    props: { animationName: "move" },
    expectedName: "move"
  }];

  for (let { desc, animation, props, expectedName } of TEST_DATA) {
    info(desc);
    for (let key in props) {
      animation[key] = props[key];
    }
    let actor = AnimationPlayerActor({}, animation);
    Assert.equal(actor.getName(), expectedName);
  }
}
