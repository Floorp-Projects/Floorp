/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test the accessible highlighter's infobar content.

const {
  truncateString,
} = require("resource://devtools/shared/inspector/utils.js");
const {
  MAX_STRING_LENGTH,
} = require("resource://devtools/server/actors/highlighters/utils/accessibility.js");

add_task(async function () {
  const { target, walker, parentAccessibility, a11yWalker } =
    await initAccessibilityFrontsForUrl(
      MAIN_DOMAIN + "doc_accessibility_infobar.html"
    );

  info("Button front checks");
  await checkNameAndRole(walker, "#button", a11yWalker, "Accessible Button");

  info("Front with long name checks");
  await checkNameAndRole(
    walker,
    "#h1",
    a11yWalker,
    "Lorem ipsum dolor sit ame" + "\u2026" + "e et dolore magna aliqua."
  );

  await waitForA11yShutdown(parentAccessibility);
  await target.destroy();
  gBrowser.removeCurrentTab();
});

/**
 * A helper function for testing the accessible's displayed name and roles.
 *
 * @param  {Object} walker
 *         The DOM walker.
 * @param  {String} querySelector
 *         The selector for the node to retrieve accessible from.
 * @param  {Object} a11yWalker
 *         The accessibility walker.
 * @param  {String} expectedName
 *         Expected string content for displaying the accessible's name.
 *         We are testing this in particular because name can be truncated.
 */
async function checkNameAndRole(
  walker,
  querySelector,
  a11yWalker,
  expectedName
) {
  const node = await walker.querySelector(walker.rootNode, querySelector);
  const accessibleFront = await a11yWalker.getAccessibleFor(node);

  const { name, role } = accessibleFront;
  const onHighlightEvent = a11yWalker.once("highlighter-event");

  await a11yWalker.highlightAccessible(accessibleFront);
  const { options } = await onHighlightEvent;
  is(options.name, name, "Accessible highlight has correct name option");
  is(options.role, role, "Accessible highlight has correct role option");

  is(
    `"${truncateString(name, MAX_STRING_LENGTH)}"`,
    `"${expectedName}"`,
    "Accessible has correct displayed name."
  );
}
