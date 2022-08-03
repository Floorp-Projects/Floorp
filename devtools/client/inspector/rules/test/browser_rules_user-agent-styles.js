/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that user agent styles are inspectable via rule view if
// it is preffed on.

var PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const { PrefObserver } = require("devtools/client/shared/prefs");

const TEST_URI = URL_ROOT + "doc_author-sheet.html";

const TEST_DATA = [
  {
    selector: "blockquote",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "pre",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "input[type=range]",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "input[type=number]",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "input[type=color]",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "input[type=text]",
    numUserRules: 1,
    numUARules: 0,
  },
  {
    selector: "progress",
    numUserRules: 1,
    numUARules: 0,
  },
  // Note that some tests below assume that the "a" selector is the
  // last test in TEST_DATA.
  {
    selector: "a",
    numUserRules: 3,
    numUARules: 0,
  },
];

add_task(async function() {
  // Bug 1517210: GC heuristics are broken for this test, so that the test ends up
  // running out of memory if we don't force to reduce the GC side before/after the test.
  Cu.forceShrinkingGC();

  requestLongerTimeout(4);

  info("Starting the test with the pref set to true before toolbox is opened");
  await setUserAgentStylesPref(true);

  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Making sure that UA styles are visible on initial load");
  await userAgentStylesVisible(inspector, view);

  info("Making sure that setting the pref to false hides UA styles");
  await setUserAgentStylesPref(false);
  await userAgentStylesNotVisible(inspector, view);

  info("Making sure that resetting the pref to true shows UA styles again");
  await setUserAgentStylesPref(true);
  await userAgentStylesVisible(inspector, view);

  info("Resetting " + PREF_UA_STYLES);
  Services.prefs.clearUserPref(PREF_UA_STYLES);

  // Bug 1517210: GC heuristics are broken for this test, so that the test ends up
  // running out of memory if we don't force to reduce the GC side before/after the test.
  Cu.forceShrinkingGC();
});

async function setUserAgentStylesPref(val) {
  info("Setting the pref " + PREF_UA_STYLES + " to: " + val);

  // Reset the pref and wait for PrefObserver to callback so UI
  // has a chance to get updated.
  const prefObserver = new PrefObserver("devtools.");
  const oncePrefChanged = new Promise(resolve => {
    prefObserver.on(PREF_UA_STYLES, onPrefChanged);

    function onPrefChanged() {
      prefObserver.off(PREF_UA_STYLES, onPrefChanged);
      resolve();
    }
  });
  Services.prefs.setBoolPref(PREF_UA_STYLES, val);
  await oncePrefChanged;
}

async function userAgentStylesVisible(inspector, view) {
  info("Making sure that user agent styles are currently visible");

  let userRules;
  let uaRules;

  for (const data of TEST_DATA) {
    await selectNode(data.selector, inspector);
    await compareAppliedStylesWithUI(inspector, view, "ua");

    userRules = view._elementStyle.rules.filter(rule => rule.editor.isEditable);
    uaRules = view._elementStyle.rules.filter(rule => !rule.editor.isEditable);
    is(userRules.length, data.numUserRules, "Correct number of user rules");
    ok(uaRules.length > data.numUARules, "Has UA rules");
  }

  ok(
    userRules.some(rule => rule.matchedSelectors.length === 1),
    "There is an inline style for element in user styles"
  );

  // These tests rely on the "a" selector being the last test in
  // TEST_DATA.
  ok(
    uaRules.some(rule => {
      return rule.matchedSelectors.includes(":any-link");
    }),
    "There is a rule for :any-link"
  );
  ok(
    uaRules.some(rule => {
      return rule.matchedSelectors.includes(":link");
    }),
    "There is a rule for :link"
  );
  ok(
    uaRules.some(rule => {
      return rule.matchedSelectors.length === 1;
    }),
    "Inline styles for ua styles"
  );
}

async function userAgentStylesNotVisible(inspector, view) {
  info("Making sure that user agent styles are not currently visible");

  let userRules;
  let uaRules;

  for (const data of TEST_DATA) {
    await selectNode(data.selector, inspector);
    await compareAppliedStylesWithUI(inspector, view);

    userRules = view._elementStyle.rules.filter(rule => rule.editor.isEditable);
    uaRules = view._elementStyle.rules.filter(rule => !rule.editor.isEditable);
    is(userRules.length, data.numUserRules, "Correct number of user rules");
    is(uaRules.length, data.numUARules, "No UA rules");
  }
}

async function compareAppliedStylesWithUI(inspector, view, filter) {
  info("Making sure that UI is consistent with pageStyle.getApplied");

  const pageStyle = inspector.selection.nodeFront.inspectorFront.pageStyle;
  let entries = await pageStyle.getApplied(inspector.selection.nodeFront, {
    inherited: true,
    matchedSelectors: true,
    filter,
  });

  // We may see multiple entries that map to a given rule; filter the
  // duplicates here to match what the UI does.
  const entryMap = new Map();
  for (const entry of entries) {
    entryMap.set(entry.rule, entry);
  }
  entries = [...entryMap.values()];

  const elementStyle = view._elementStyle;
  is(
    elementStyle.rules.length,
    entries.length,
    "Should have correct number of rules (" + entries.length + ")"
  );

  entries = entries.sort((a, b) => {
    return (a.pseudoElement || "z") > (b.pseudoElement || "z");
  });

  entries.forEach((entry, i) => {
    const elementStyleRule = elementStyle.rules[i];
    is(
      !!elementStyleRule.inherited,
      !!entry.inherited,
      "Same inherited (" + entry.inherited + ")"
    );
    is(
      elementStyleRule.isSystem,
      entry.isSystem,
      "Same isSystem (" + entry.isSystem + ")"
    );
    is(
      elementStyleRule.editor.isEditable,
      !entry.isSystem,
      "Editor isEditable opposite of UA (" + entry.isSystem + ")"
    );
  });
}
