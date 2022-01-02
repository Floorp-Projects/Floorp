/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

gReduceMotionOverride = false;

function promiseWidgetAnimationOut(aNode) {
  let animationNode = aNode;
  if (
    animationNode.tagName != "toolbaritem" &&
    animationNode.tagName != "toolbarbutton"
  ) {
    animationNode = animationNode.closest("toolbaritem");
  }
  if (animationNode.parentNode.id.startsWith("wrapper-")) {
    animationNode = animationNode.parentNode;
  }
  return new Promise(resolve => {
    animationNode.addEventListener(
      "animationend",
      function cleanupWidgetAnimationOut(e) {
        if (
          e.animationName == "widget-animate-out" &&
          e.target.id == animationNode.id
        ) {
          animationNode.removeEventListener(
            "animationend",
            cleanupWidgetAnimationOut
          );
          ok(true, "The widget`s animationend should have happened");
          resolve();
        }
      }
    );
  });
}

function promiseOverflowAnimationEnd() {
  return new Promise(resolve => {
    let overflowButton = document.getElementById("nav-bar-overflow-button");
    overflowButton.addEventListener(
      "animationend",
      function cleanupOverflowAnimationOut(event) {
        if (event.animationName == "overflow-animation") {
          overflowButton.removeEventListener(
            "animationend",
            cleanupOverflowAnimationOut
          );
          ok(
            true,
            "The overflow button`s animationend event should have happened"
          );
          resolve();
        }
      }
    );
  });
}

// Right-click on the stop/reload button, use the context menu to move it to the overflow menu.
// The button should animate out, and the overflow menu should animate upon adding.
add_task(async function() {
  let stopReloadButton = document.getElementById("stop-reload-button");
  let contextMenu = document.getElementById("toolbar-context-menu");
  let shownPromise = popupShown(contextMenu);
  EventUtils.synthesizeMouseAtCenter(stopReloadButton, {
    type: "contextmenu",
    button: 2,
  });
  await shownPromise;

  contextMenu.querySelector(".customize-context-moveToPanel").click();
  await contextMenu.hidePopup();

  await Promise.all([
    promiseWidgetAnimationOut(stopReloadButton),
    promiseOverflowAnimationEnd(),
  ]);
  ok(true, "The widget and overflow animations should have both happened.");
});

registerCleanupFunction(CustomizableUI.reset);
