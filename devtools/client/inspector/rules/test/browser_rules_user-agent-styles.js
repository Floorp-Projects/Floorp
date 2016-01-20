/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that user agent styles are inspectable via rule view if
// it is preffed on.

var PREF_UA_STYLES = "devtools.inspector.showUserAgentStyles";
const { PrefObserver } = require("devtools/client/styleeditor/utils");

const TEST_URI = URL_ROOT + "doc_author-sheet.html";

const TEST_DATA = [
  {
    selector: "blockquote",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "pre",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "input[type=range]",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "input[type=number]",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "input[type=color]",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "input[type=text]",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "progress",
    numUserRules: 1,
    numUARules: 0
  },
  {
    selector: "a",
    numUserRules: 3,
    numUARules: 0
  }
];

add_task(function*() {
  requestLongerTimeout(2);

  info("Starting the test with the pref set to true before toolbox is opened");
  yield setUserAgentStylesPref(true);

  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  info("Making sure that UA styles are visible on initial load");
  yield userAgentStylesVisible(inspector, view);

  info("Making sure that setting the pref to false hides UA styles");
  yield setUserAgentStylesPref(false);
  yield userAgentStylesNotVisible(inspector, view);

  info("Making sure that resetting the pref to true shows UA styles again");
  yield setUserAgentStylesPref(true);
  yield userAgentStylesVisible(inspector, view);

  info("Resetting " + PREF_UA_STYLES);
  Services.prefs.clearUserPref(PREF_UA_STYLES);
});

function* setUserAgentStylesPref(val) {
  info("Setting the pref " + PREF_UA_STYLES + " to: " + val);

  // Reset the pref and wait for PrefObserver to callback so UI
  // has a chance to get updated.
  let oncePrefChanged = promise.defer();
  let prefObserver = new PrefObserver("devtools.");
  prefObserver.on(PREF_UA_STYLES, oncePrefChanged.resolve);
  Services.prefs.setBoolPref(PREF_UA_STYLES, val);
  yield oncePrefChanged.promise;
  prefObserver.off(PREF_UA_STYLES, oncePrefChanged.resolve);
}

function* userAgentStylesVisible(inspector, view) {
  info("Making sure that user agent styles are currently visible");

  let userRules;
  let uaRules;

  for (let data of TEST_DATA) {
    yield selectNode(data.selector, inspector);
    yield compareAppliedStylesWithUI(inspector, view, "ua");

    userRules = view._elementStyle.rules.filter(rule=>rule.editor.isEditable);
    uaRules = view._elementStyle.rules.filter(rule=>!rule.editor.isEditable);
    is(userRules.length, data.numUserRules, "Correct number of user rules");
    ok(uaRules.length > data.numUARules, "Has UA rules");
  }

  ok(userRules.some(rule=> rule.matchedSelectors.length === 1),
    "There is an inline style for element in user styles");

  ok(uaRules.some(rule=> rule.matchedSelectors.indexOf(":-moz-any-link")),
    "There is a rule for :-moz-any-link");
  ok(uaRules.some(rule=> rule.matchedSelectors.indexOf("*|*:link")),
    "There is a rule for *|*:link");
  ok(uaRules.some(rule=> rule.matchedSelectors.length === 1),
    "Inline styles for ua styles");
}

function* userAgentStylesNotVisible(inspector, view) {
  info("Making sure that user agent styles are not currently visible");

  let userRules;
  let uaRules;

  for (let data of TEST_DATA) {
    yield selectNode(data.selector, inspector);
    yield compareAppliedStylesWithUI(inspector, view);

    userRules = view._elementStyle.rules.filter(rule=>rule.editor.isEditable);
    uaRules = view._elementStyle.rules.filter(rule=>!rule.editor.isEditable);
    is(userRules.length, data.numUserRules, "Correct number of user rules");
    is(uaRules.length, data.numUARules, "No UA rules");
  }
}

function* compareAppliedStylesWithUI(inspector, view, filter) {
  info("Making sure that UI is consistent with pageStyle.getApplied");

  let entries = yield inspector.pageStyle.getApplied(
    inspector.selection.nodeFront, {
    inherited: true,
    matchedSelectors: true,
    filter: filter
  });

  // We may see multiple entries that map to a given rule; filter the
  // duplicates here to match what the UI does.
  let entryMap = new Map();
  for (let entry of entries) {
    entryMap.set(entry.rule, entry);
  }
  entries = [...entryMap.values()];

  let elementStyle = view._elementStyle;
  is(elementStyle.rules.length, entries.length,
    "Should have correct number of rules (" + entries.length + ")");

  entries = entries.sort((a, b) => {
    return (a.pseudoElement || "z") > (b.pseudoElement || "z");
  });

  entries.forEach((entry, i) => {
    let elementStyleRule = elementStyle.rules[i];
    is(elementStyleRule.inherited, entry.inherited,
      "Same inherited (" + entry.inherited + ")");
    is(elementStyleRule.isSystem, entry.isSystem,
      "Same isSystem (" + entry.isSystem + ")");
    is(elementStyleRule.editor.isEditable, !entry.isSystem,
      "Editor isEditable opposite of UA (" + entry.isSystem + ")");
  });
}
