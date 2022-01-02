/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint no-unused-vars: [2, {"vars": "local"}] */
/* import-globals-from ../../../shared/test/shared-head.js */
/* import-globals-from ../../test/head.js */
"use strict";

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

Services.prefs.setIntPref("devtools.toolbox.footer.height", 350);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
});

/**
 * Is the given node visible in the page (rendered in the frame tree).
 * @param {DOMNode}
 * @return {Boolean}
 */
function isNodeVisible(node) {
  return !!node.getClientRects().length;
}

/**
 * Wait for the boxmodel-view-updated event.
 *
 * @param  {InspectorPanel} inspector
 *         The instance of InspectorPanel currently loaded in the toolbox.
 * @param  {Boolean} waitForSelectionUpdate
 *         Should the boxmodel-view-updated event come from a new selection.
 * @return {Promise} a promise
 */
async function waitForUpdate(inspector, waitForSelectionUpdate) {
  /**
   * While the highlighter is visible (mouse over the fields of the box model editor),
   * reflow events are prevented; see ReflowActor -> setIgnoreLayoutChanges()
   * The box model view updates in response to reflow events.
   * To ensure reflow events are fired, hide the highlighter.
   */
  await inspector.highlighters.hideHighlighterType(
    inspector.highlighters.TYPES.BOXMODEL
  );

  return new Promise(resolve => {
    inspector.on("boxmodel-view-updated", function onUpdate(reasons) {
      // Wait for another update event if we are waiting for a selection related event.
      if (waitForSelectionUpdate && !reasons.includes("new-selection")) {
        return;
      }

      inspector.off("boxmodel-view-updated", onUpdate);
      resolve();
    });
  });
}

/**
 * Wait for both boxmode-view-updated and markuploaded events.
 *
 * @return {Promise} a promise that resolves when both events have been received.
 */
function waitForMarkupLoaded(inspector) {
  return Promise.all([
    waitForUpdate(inspector),
    inspector.once("markuploaded"),
  ]);
}

function getStyle(browser, selector, propertyName) {
  return SpecialPowers.spawn(browser, [selector, propertyName], async function(
    _selector,
    _propertyName
  ) {
    return content.document
      .querySelector(_selector)
      .style.getPropertyValue(_propertyName);
  });
}

function setStyle(browser, selector, propertyName, value) {
  return SpecialPowers.spawn(
    browser,
    [selector, propertyName, value],
    async function(_selector, _propertyName, _value) {
      content.document.querySelector(_selector).style[_propertyName] = _value;
    }
  );
}

/**
 * The box model doesn't participate in the inspector's update mechanism, so simply
 * calling the default selectNode isn't enough to guarantee that the box model view has
 * finished updating. We also need to wait for the "boxmodel-view-updated" event.
 */
var _selectNode = selectNode;
selectNode = async function(node, inspector, reason) {
  const onUpdate = waitForUpdate(inspector, true);
  await _selectNode(node, inspector, reason);
  await onUpdate;
};

/**
 * Wait until the provided element's text content matches the provided text.
 * Based on the waitFor helper, see documentation in
 * devtools/client/shared/test/shared-head.js
 *
 * @param {DOMNode} element
 *        The element to check.
 * @param {String} expectedText
 *        The text that is expected to be set as textContent of the element.
 */
async function waitForElementTextContent(element, expectedText) {
  await waitFor(
    () => element.textContent === expectedText,
    `Couldn't get "${expectedText}" as the text content of the given element`
  );
  ok(true, `Found the expected text (${expectedText}) for the given element`);
}
