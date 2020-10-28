/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that all customizable buttons have labels and icons.
 *
 * This is primarily designed to ensure we don't end up with items without
 * labels in customize mode. In the past, this has happened due to race
 * conditions, where labels would be correct if and only if the item had
 * already been moved into a toolbar or panel in the main UI before
 * (forcing it to be constructed and any fluent identifiers to be localized
 * and applied).
 * We use a new window to ensure that earlier tests using some of the widgets
 * in the palette do not influence our checks to see that such items get
 * labels, "even" if the first time they're rendered is in customize mode's
 * palette.
 */
add_task(async function test_all_buttons_have_labels() {
  let win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(async () => {
    await endCustomizing(win);
    return BrowserTestUtils.closeWindow(win);
  });
  await startCustomizing(win);
  let { palette } = win.gNavToolbox;
  // Wait for things to paint.
  await TestUtils.waitForCondition(() => {
    return !!Array.from(palette.querySelectorAll(".toolbarbutton-icon")).filter(
      n => {
        let rect = n.getBoundingClientRect();
        return rect.height > 0 && rect.width > 0;
      }
    ).length;
  }, "Must start rendering icons.");

  for (let wrapper of palette.children) {
    if (wrapper.hasAttribute("title")) {
      ok(true, wrapper.firstElementChild.id + " has a label.");
    } else {
      info(
        `${wrapper.firstElementChild.id} doesn't seem to have a label, waiting.`
      );
      await BrowserTestUtils.waitForAttribute("title", wrapper);
      ok(
        wrapper.hasAttribute("title"),
        wrapper.firstElementChild.id + " has a label."
      );
    }
    let icons = Array.from(wrapper.querySelectorAll(".toolbarbutton-icon"));
    // If there are icons, at least one must be visible
    // (not everything necessarily has one, e.g. the search bar has no icon)
    if (icons.length) {
      let visibleIcons = icons.filter(n => {
        let rect = n.getBoundingClientRect();
        return rect.height > 0 && rect.width > 0;
      });
      Assert.greater(
        visibleIcons.length,
        0,
        `${wrapper.firstElementChild.id} should have at least one visible icon.`
      );
    }
  }
});
