/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// A helper frame-script for brower/devtools/animationinspector tests.

/**
 * Toggle (play or pause) one of the animation players of a given node.
 * @param {Object} data
 * - {Number} animationIndex The index of the node's animationPlayers to play or pause
 * @param {Object} objects
 * - {DOMNode} node The node to use
 */
addMessageListener("Test:ToggleAnimationPlayer", function(msg) {
  let {animationIndex, pause} = msg.data;
  let {node} = msg.objects;

  let player = node.getAnimationPlayers()[animationIndex];
  if (pause) {
    player.pause();
  } else {
    player.play();
  }

  sendAsyncMessage("Test:ToggleAnimationPlayer");
});
