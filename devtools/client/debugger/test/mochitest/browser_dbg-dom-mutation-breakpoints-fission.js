/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests dom mutation breakpoints with a remote frame.

"use strict";

// Import helpers for the inspector
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

/**
 * The test page contains an INPUT and a BUTTON.
 *
 * On "click", the button will update the disabled attribute of the input.
 * Updating the attribute via SpecialPowers.spawn would not trigger the DOM
 * breakpoint, that's why we need the page itself to have a way of updating
 * the attribute.
 */
const TEST_COM_URI = `https://example.com/document-builder.sjs?html=${encodeURI(
  `<input disabled=""/>
     <button onclick="document.querySelector('input').toggleAttribute('disabled')">
       click me
     </button>`
)}`;

// Embed the example.com test page in an example.org iframe.
const TEST_URI = `https://example.org/document-builder.sjs?html=
<iframe src="${encodeURI(TEST_COM_URI)}"></iframe><body>`;

add_task(async function () {
  await pushPref("devtools.debugger.dom-mutation-breakpoints-visible", true);

  const { inspector, toolbox } = await openInspectorForURL(TEST_URI);

  await selectNodeInFrames(["iframe", "input"], inspector);
  info("Adding DOM mutation breakpoints to body");
  const allMenuItems = openContextMenuAndGetAllItems(inspector);

  const attributeMenuItem = allMenuItems.find(
    item => item.id === "node-menu-mutation-breakpoint-attribute"
  );
  attributeMenuItem.click();

  info("Switches over to the debugger pane");
  await toolbox.selectTool("jsdebugger");

  const dbg = createDebuggerContext(toolbox);

  info("Changing attribute to trigger debugger pause");
  const frameBC = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function () {
      return content.document.querySelector("iframe").browsingContext;
    }
  );

  info("Confirms that one DOM mutation breakpoint exists");
  const mutationItem = await waitForElement(dbg, "domMutationItem");
  ok(mutationItem, "A DOM mutation breakpoint exists");

  mutationItem.scrollIntoView();

  info("Enabling and disabling the DOM mutation breakpoint works");
  const checkbox = mutationItem.querySelector("input");
  checkbox.click();
  await waitFor(() => !checkbox.checked);

  info("Click the button in the remote iframe, should not hit the breakpoint");
  BrowserTestUtils.synthesizeMouseAtCenter("button", {}, frameBC);

  info("Wait until the input is enabled");
  await asyncWaitUntil(() =>
    SpecialPowers.spawn(
      frameBC,
      [],
      () => !content.document.querySelector("input").disabled
    )
  );
  assertNotPaused(dbg, "DOM breakpoint should not have been hit");

  info("Restore the disabled attribute");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    return SpecialPowers.spawn(
      content.document.querySelector("iframe"),
      [],
      () =>
        !content.document.querySelector("input").setAttribute("disabled", "")
    );
  });

  checkbox.click();
  await waitFor(() => checkbox.checked);

  info("Click the button in the remote iframe, to trigger the breakpoint");
  BrowserTestUtils.synthesizeMouseAtCenter("button", {}, frameBC);

  info("Wait for paused");
  await waitForPaused(dbg);
  info("Resume");
  await resume(dbg);

  info("Removing breakpoints works");
  dbg.win.document.querySelector(".dom-mutation-list .close-btn").click();
  await waitForAllElements(dbg, "domMutationItem", 0, true);
});
