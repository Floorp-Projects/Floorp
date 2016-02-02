/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the AnimationPlayerActor provides the correct name for an
// animation. Whether this animation is a CSSAnimation, CSSTransition or a
// script-based animation that has been given an id, or even a CSSAnimation that
// has been given an id.

const TEST_DATA = [{
  selector: ".simple-animation",
  animationIndex: 0,
  expectedName: "move"
}, {
  selector: ".transition",
  animationIndex: 0,
  expectedName: "width"
}, {
  selector: ".script-generated",
  animationIndex: 0,
  expectedName: "custom-animation-name"
}, {
  selector: ".delayed-animation",
  animationIndex: 0,
  expectedName: "cssanimation-custom-name"
}];

add_task(function*() {
  let {client, walker, animations} =
    yield initAnimationsFrontForUrl(MAIN_DOMAIN + "animation.html");

  for (let {selector, animationIndex, expectedName} of TEST_DATA) {
    let {name} = yield getAnimationStateForNode(walker, animations, selector,
                                                animationIndex);
    is(name, expectedName, "The animation has the expected name");
  }

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});

function* getAnimationStateForNode(walker, animations, nodeSelector, index) {
  let node = yield walker.querySelector(walker.rootNode, nodeSelector);
  let players = yield animations.getAnimationPlayersForNode(node);
  let player = players[index];
  yield player.ready();
  let state = yield player.getCurrentState();
  return state;
}
