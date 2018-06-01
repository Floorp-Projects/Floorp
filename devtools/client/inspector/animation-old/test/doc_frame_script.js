/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals addMessageListener, sendAsyncMessage */

"use strict";

// A helper frame-script for browser/devtools/animationinspector tests.

/**
 * Toggle (play or pause) one of the animation players of a given node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {Number} animationIndex The index of the node's animationPlayers to play
 *   or pause
 * - {Boolean} pause True to pause the animation, false to play.
 */
addMessageListener("Test:ToggleAnimationPlayer", function(msg) {
  const {selector, animationIndex, pause} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  const animation = node.getAnimations()[animationIndex];
  if (pause) {
    animation.pause();
  } else {
    animation.play();
  }

  sendAsyncMessage("Test:ToggleAnimationPlayer");
});

/**
 * Change the currentTime of one of the animation players of a given node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {Number} animationIndex The index of the node's animationPlayers to change.
 * - {Number} currentTime The current time to set.
 */
addMessageListener("Test:SetAnimationPlayerCurrentTime", function(msg) {
  const {selector, animationIndex, currentTime} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  const animation = node.getAnimations()[animationIndex];
  animation.currentTime = currentTime;

  sendAsyncMessage("Test:SetAnimationPlayerCurrentTime");
});

/**
 * Change the playbackRate of one of the animation players of a given node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {Number} animationIndex The index of the node's animationPlayers to change.
 * - {Number} playbackRate The rate to set.
 */
addMessageListener("Test:SetAnimationPlayerPlaybackRate", function(msg) {
  const {selector, animationIndex, playbackRate} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  const player = node.getAnimations()[animationIndex];
  player.playbackRate = playbackRate;

  sendAsyncMessage("Test:SetAnimationPlayerPlaybackRate");
});

/**
 * Get the current playState of an animation player on a given node.
 * @param {Object} data
 * - {String} selector The CSS selector to get the node (can be a "super"
 *   selector).
 * - {Number} animationIndex The index of the node's animationPlayers to check
 */
addMessageListener("Test:GetAnimationPlayerState", function(msg) {
  const {selector, animationIndex} = msg.data;
  const node = superQuerySelector(selector);
  if (!node) {
    return;
  }

  const animation = node.getAnimations()[animationIndex];
  animation.ready.then(() => {
    sendAsyncMessage("Test:GetAnimationPlayerState", animation.playState);
  });
});

/**
 * Like document.querySelector but can go into iframes too.
 * ".container iframe || .sub-container div" will first try to find the node
 * matched by ".container iframe" in the root document, then try to get the
 * content document inside it, and then try to match ".sub-container div" inside
 * this document.
 * Any selector coming before the || separator *MUST* match a frame node.
 * @param {String} superSelector.
 * @return {DOMNode} The node, or null if not found.
 */
function superQuerySelector(superSelector, root = content.document) {
  const frameIndex = superSelector.indexOf("||");
  if (frameIndex === -1) {
    return root.querySelector(superSelector);
  }

  const rootSelector = superSelector.substring(0, frameIndex).trim();
  const childSelector = superSelector.substring(frameIndex + 2).trim();
  root = root.querySelector(rootSelector);
  if (!root || !root.contentWindow) {
    return null;
  }

  return superQuerySelector(childSelector, root.contentWindow.document);
}
