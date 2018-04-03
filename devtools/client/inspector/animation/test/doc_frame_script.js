/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* globals addMessageListener, sendAsyncMessage */

"use strict";

// A helper frame-script for animation inspector.

addMessageListener("Test:RemoveAnimatedElementsExcept", function(msg) {
  const { selectors } = msg.data;

  for (const animation of content.document.getAnimations()) {
    if (isRemovableElement(animation, selectors)) {
      animation.effect.target.remove();
    }
  }

  sendAsyncMessage("Test:RemoveAnimatedElementsExcept");
});

function isRemovableElement(animation, selectors) {
  for (const selector of selectors) {
    if (animation.effect.target.matches(selector)) {
      return false;
    }
  }

  return true;
}
