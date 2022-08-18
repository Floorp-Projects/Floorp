/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kPasteMenuPopupId = "clipboardReadTextPasteMenuPopup";
const kPasteMenuItemId = "clipboardReadTextPasteMenuItem";

function promiseWritingRandomTextToClipboard() {
  const clipboardText = "X" + Math.random();
  return navigator.clipboard.writeText(clipboardText).then(() => {
    return clipboardText;
  });
}

function promiseBrowserReflow() {
  return new Promise(resolve =>
    requestAnimationFrame(() => requestAnimationFrame(resolve))
  );
}

function waitForPasteMenuPopupEvent(aEventSuffix) {
  // The element with id `kPasteMenuPopupId` is inserted dynamically, hence
  // calling `BrowserTestUtils.waitForEvent` instead of
  // `BrowserTestUtils.waitForPopupEvent`.
  return BrowserTestUtils.waitForEvent(
    document,
    "popup" + aEventSuffix,
    false /* capture */,
    e => {
      return e.target.getAttribute("id") == kPasteMenuPopupId;
    }
  );
}

function promisePasteButtonIsShown() {
  return waitForPasteMenuPopupEvent("shown").then(() => {
    ok(true, "Witnessed 'popupshown' event for 'Paste' button.");

    const pasteButton = document.getElementById(kPasteMenuItemId);

    return promiseBrowserReflow().then(() => {
      return coordinatesRelativeToScreen({
        target: pasteButton,
        offsetX: 0,
        offsetY: 0,
      });
    });
  });
}

function promisePasteButtonIsHidden() {
  return waitForPasteMenuPopupEvent("hidden").then(() => {
    ok(true, "Witnessed 'popuphidden' event for 'Paste' button.");
    return promiseBrowserReflow();
  });
}

function promiseClickPasteButton() {
  const pasteButton = document.getElementById(kPasteMenuItemId);
  let promise = BrowserTestUtils.waitForEvent(pasteButton, "click");
  EventUtils.synthesizeMouseAtCenter(pasteButton, {});
  return promise;
}

function getMouseCoordsRelativeToScreenInDevicePixels() {
  let mouseXInCSSPixels = {};
  let mouseYInCSSPixels = {};
  window.windowUtils.getLastOverWindowPointerLocationInCSSPixels(
    mouseXInCSSPixels,
    mouseYInCSSPixels
  );

  return {
    x:
      (mouseXInCSSPixels.value + window.mozInnerScreenX) *
      window.devicePixelRatio,
    y:
      (mouseYInCSSPixels.value + window.mozInnerScreenY) *
      window.devicePixelRatio,
  };
}

function isCloselyLeftOnTopOf(aCoordsP1, aCoordsP2, aDelta = 10) {
  return (
    Math.abs(aCoordsP2.x - aCoordsP1.x) < aDelta &&
    Math.abs(aCoordsP2.y - aCoordsP1.y) < aDelta
  );
}
