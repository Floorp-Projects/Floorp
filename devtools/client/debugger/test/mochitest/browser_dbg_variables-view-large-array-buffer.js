/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the variables view remains responsive when faced with
 * huge ammounts of data.
 */

"use strict";

const TAB_URL = EXAMPLE_URL + "doc_large-array-buffer.html";
const {ELLIPSIS} = require("devtools/shared/l10n");


var gTab, gPanel, gDebugger, gVariables;

function test() {
  // this test does a lot of work on large objects, default 45s is not enough
  requestLongerTimeout(4);

  let options = {
    source: TAB_URL,
    line: 1
  };
  initDebugger(TAB_URL, options).then(([aTab,, aPanel]) => {
    gTab = aTab;
    gPanel = aPanel;
    gDebugger = gPanel.panelWin;
    gVariables = gDebugger.DebuggerView.Variables;

    waitForCaretAndScopes(gPanel, 28, 1)
      .then(() => performTests())
      .then(() => resumeDebuggerThenCloseAndFinish(gPanel))
      .catch(error => {
        ok(false, "Got an error: " + error.message + "\n" + error.stack);
      });

    generateMouseClickInTab(gTab, "content.document.querySelector('button')");
  });
}

const VARS_TO_TEST = [
  {
    varName: "buffer",
    stringified: "ArrayBuffer",
    doNotExpand: true
  },
  {
    varName: "largeArray",
    stringified: "Int8Array[10000]",
    extraProps: [
      [ "buffer", "ArrayBuffer" ],
      [ "byteLength", "10000" ],
      [ "byteOffset", "0" ],
      [ "length", "10000" ],
      [ "__proto__", "Int8ArrayPrototype" ]
    ]
  },
  {
    varName: "largeObject",
    stringified: "Object[10000]",
    extraProps: [
      [ "__proto__", "Object" ]
    ]
  },
  {
    varName: "largeMap",
    stringified: "Map[10000]",
    hasEntries: true,
    extraProps: [
      [ "size", "10000" ],
      [ "__proto__", "Object" ]
    ]
  },
  {
    varName: "largeSet",
    stringified: "Set[10000]",
    hasEntries: true,
    extraProps: [
      [ "size", "10000" ],
      [ "__proto__", "Object" ]
    ]
  }
];

const PAGE_RANGES = [
  [0, 2499], [2500, 4999], [5000, 7499], [7500, 9999]
];

function toPageNames(ranges) {
  return ranges.map(([ from, to ]) => "[" + from + ELLIPSIS + to + "]");
}

function performTests() {
  let localScope = gVariables.getScopeAtIndex(0);

  return promise.all(VARS_TO_TEST.map(spec => {
    let { varName, stringified, doNotExpand } = spec;

    let variable = localScope.get(varName);
    ok(variable,
      `There should be a '${varName}' variable present in the scope.`);

    is(variable.target.querySelector(".name").getAttribute("value"), varName,
      `Should have the right property name for '${varName}'.`);
    is(variable.target.querySelector(".value").getAttribute("value"), stringified,
      `Should have the right property value for '${varName}'.`);
    ok(variable.target.querySelector(".value").className.includes("token-other"),
      `Should have the right token class for '${varName}'.`);

    is(variable.expanded, false,
      `The '${varName}' variable shouldn't be expanded.`);

    if (doNotExpand) {
      return promise.resolve();
    }

    return variable.expand()
      .then(() => verifyFirstLevel(variable, spec));
  }));
}

// In objects and arrays, the sliced pages are at the top-level of
// the expanded object, but with Maps and Sets, we have to expand
// <entries> first and look there.
function getExpandedPages(variable, hasEntries) {
  let expandedPages = promise.defer();
  if (hasEntries) {
    let entries = variable.get("<entries>");
    ok(entries, "<entries> retrieved");
    entries.expand().then(() => expandedPages.resolve(entries));
  } else {
    expandedPages.resolve(variable);
  }

  return expandedPages.promise;
}

function verifyFirstLevel(variable, spec) {
  let { varName, hasEntries, extraProps } = spec;

  let enums = variable._enum.childNodes;
  let nonEnums = variable._nonenum.childNodes;

  is(enums.length, hasEntries ? 1 : 4,
    `The '${varName}' contains the right number of enumerable elements.`);
  is(nonEnums.length, extraProps.length,
    `The '${varName}' contains the right number of non-enumerable elements.`);

  // the sliced pages begin after <entries> row
  let pagesOffset = hasEntries ? 1 : 0;
  let expandedPages = getExpandedPages(variable, hasEntries);

  return expandedPages.then((pagesList) => {
    toPageNames(PAGE_RANGES).forEach((pageName, i) => {
      let index = i + pagesOffset;

      is(pagesList.target.querySelectorAll(".variables-view-property .name")[index].getAttribute("value"),
        pageName, `The page #${i + 1} in the '${varName}' is named correctly.`);
      is(pagesList.target.querySelectorAll(".variables-view-property .value")[index].getAttribute("value"),
        "", `The page #${i + 1} in the '${varName}' should not have a corresponding value.`);
    });
  }).then(() => {
    extraProps.forEach(([ propName, propValue ], i) => {
      // the extra props start after the 4 pages
      let index = i + pagesOffset + 4;

      is(variable.target.querySelectorAll(".variables-view-property .name")[index].getAttribute("value"),
        propName, `The other properties in '${varName}' are named correctly.`);
      is(variable.target.querySelectorAll(".variables-view-property .value")[index].getAttribute("value"),
        propValue, `The other properties in '${varName}' have the correct value.`);
    });
  }).then(() => verifyNextLevels(variable, spec));
}

function verifyNextLevels(variable, spec) {
  let { varName, hasEntries } = spec;

  // the entries are already expanded in verifyFirstLevel
  let pagesList = hasEntries ? variable.get("<entries>") : variable;

  let lastPage = pagesList.get(toPageNames(PAGE_RANGES)[3]);
  ok(lastPage, `The last page in the 1st level of '${varName}' was retrieved successfully.`);

  return lastPage.expand()
    .then(() => verifyNextLevels2(lastPage, varName));
}

function verifyNextLevels2(lastPage1, varName) {
  const PAGE_RANGES_IN_LAST_PAGE = [
    [7500, 8124], [8125, 8749], [8750, 9374], [9375, 9999]
  ];

  let pageEnums1 = lastPage1._enum.childNodes;
  let pageNonEnums1 = lastPage1._nonenum.childNodes;
  is(pageEnums1.length, 4,
    `The last page in the 1st level of '${varName}' should contain all the created enumerable elements.`);
  is(pageNonEnums1.length, 0,
    `The last page in the 1st level of '${varName}' should not contain any non-enumerable elements.`);

  let pageNames = toPageNames(PAGE_RANGES_IN_LAST_PAGE);
  pageNames.forEach((pageName, i) => {
    is(lastPage1._enum.querySelectorAll(".variables-view-property .name")[i].getAttribute("value"),
      pageName, `The page #${i + 1} in the 2nd level of '${varName}' is named correctly.`);
  });

  let lastPage2 = lastPage1.get(pageNames[3]);
  ok(lastPage2, "The last page in the 2nd level was retrieved successfully.");

  return lastPage2.expand()
    .then(() => verifyNextLevels3(lastPage2, varName));
}

function verifyNextLevels3(lastPage2, varName) {
  let pageEnums2 = lastPage2._enum.childNodes;
  let pageNonEnums2 = lastPage2._nonenum.childNodes;
  is(pageEnums2.length, 625,
    `The last page in the 3rd level of '${varName}' should contain all the created enumerable elements.`);
  is(pageNonEnums2.length, 0,
    `The last page in the 3rd level of '${varName}' shouldn't contain any non-enumerable elements.`);

  const LEAF_ITEMS = [
    [0, 9375, 624],
    [1, 9376, 623],
    [623, 9998, 1],
    [624, 9999, 0]
  ];

  function expectedValue(name, value) {
    switch (varName) {
      case "largeArray": return 0;
      case "largeObject": return value;
      case "largeMap": return name + " \u2192 " + value;
      case "largeSet": return value;
    }
  }

  LEAF_ITEMS.forEach(([index, name, value]) => {
    is(lastPage2._enum.querySelectorAll(".variables-view-property .name")[index].getAttribute("value"),
      name, `The properties in the leaf level of '${varName}' are named correctly.`);
    is(lastPage2._enum.querySelectorAll(".variables-view-property .value")[index].getAttribute("value"),
      expectedValue(name, value), `The properties in the leaf level of '${varName}' have the correct value.`);
  });
}

registerCleanupFunction(function () {
  gTab = null;
  gPanel = null;
  gDebugger = null;
  gVariables = null;
});
