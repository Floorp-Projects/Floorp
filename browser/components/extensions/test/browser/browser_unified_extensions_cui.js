/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

loadTestSubscript("head_unified_extensions.js");

/**
 * Tests that if the addons panel is somehow open when customization mode is
 * invoked, that the panel is hidden.
 */
add_task(async function test_hide_panel_when_customizing() {
  await openExtensionsPanel();

  let panel = gUnifiedExtensions.panel;
  Assert.equal(panel.state, "open");

  let panelHidden = BrowserTestUtils.waitForPopupEvent(panel, "hidden");
  CustomizableUI.dispatchToolboxEvent("customizationstarting", {});
  await panelHidden;
  Assert.equal(panel.state, "closed");
});
