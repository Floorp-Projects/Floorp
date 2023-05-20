/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL_ROOT =
  "chrome://mochitests/content/browser/browser/extensions/formautofill/test/browser/focus-leak/";

const XUL_FRAME_URI = URL_ROOT + "doc_iframe_typecontent_input_focus.xhtml";
const INNER_HTML_FRAME_URI =
  URL_ROOT + "doc_iframe_typecontent_input_focus_frame.html";

/**
 * Check that focusing an input in a frame with type=content embedded in a xul
 * document does not leak.
 */
add_task(async function () {
  const doc = gBrowser.ownerDocument;
  const linkedBrowser = gBrowser.selectedTab.linkedBrowser;
  const browserContainer = gBrowser.getBrowserContainer(linkedBrowser);

  info("Load the test page in a frame with type content");
  const frame = doc.createXULElement("iframe");
  frame.setAttribute("type", "content");
  browserContainer.appendChild(frame);

  info("Wait for the xul iframe to be loaded");
  const onXulFrameLoad = BrowserTestUtils.waitForEvent(frame, "load", true);
  frame.setAttribute("src", XUL_FRAME_URI);
  await onXulFrameLoad;

  const panelFrame = frame.contentDocument.querySelector("#html-iframe");

  info("Wait for the html iframe to be loaded");
  const onFrameLoad = BrowserTestUtils.waitForEvent(panelFrame, "load", true);
  panelFrame.setAttribute("src", INNER_HTML_FRAME_URI);
  await onFrameLoad;

  info("Focus an input inside the iframe");
  const focusMeInput = panelFrame.contentDocument.querySelector(".focusme");
  const onFocus = BrowserTestUtils.waitForEvent(focusMeInput, "focus");
  await SimpleTest.promiseFocus(panelFrame);
  focusMeInput.focus();
  await onFocus;

  // This assert is not really meaningful, the main purpose of the test is
  // to check against leaks.
  is(
    focusMeInput,
    panelFrame.contentDocument.activeElement,
    "The .focusme input is the active element"
  );

  info("Remove the focused input");
  focusMeInput.remove();
});
