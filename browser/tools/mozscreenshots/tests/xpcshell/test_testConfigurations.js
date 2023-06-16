/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { TestRunner } = ChromeUtils.importESModule(
  "resource://test/TestRunner.sys.mjs"
);

add_task(async function capture() {
  equal(TestRunner.findComma("Toolbars,Devs"), 8);
  equal(TestRunner.findComma("Toolbars"), -1);
  equal(TestRunner.findComma("Toolbars[onlyNavBar,allToolbars],DevTools"), 32);
  equal(
    TestRunner.findComma(
      "Toolbars[onlyNavBar,allToolbars],DevTools[bottomToolbox,sideToolbox]"
    ),
    32
  );
  equal(
    TestRunner.findComma(
      "Toolbars[[onlyNavBar],[]], Tabs[ [fiveTabbed], [[[fourPinned]]] ]"
    ),
    25
  );
  equal(TestRunner.findComma("[[[[[[[[[[[[[[[[[[[[]]"), -1);
  equal(TestRunner.findComma("Preferences[[[[[,]]]]]"), -1);

  deepEqual(TestRunner.splitEnv("Toolbars"), ["Toolbars"]);
  deepEqual(TestRunner.splitEnv("Buttons,Tabs"), ["Buttons", "Tabs"]);
  deepEqual(TestRunner.splitEnv("Buttons,    Tabs"), ["Buttons", "Tabs"]);
  deepEqual(TestRunner.splitEnv("    Buttons   ,   Tabs    "), [
    "Buttons",
    "Tabs",
  ]);
  deepEqual(TestRunner.splitEnv("Toolbars[onlyNavBar,allToolbars],DevTools"), [
    "Toolbars[onlyNavBar,allToolbars]",
    "DevTools",
  ]);
  deepEqual(
    TestRunner.splitEnv(
      "Toolbars[onlyNavBar,allToolbars],DevTools[bottomToolbox]"
    ),
    ["Toolbars[onlyNavBar,allToolbars]", "DevTools[bottomToolbox]"]
  );
  deepEqual(
    TestRunner.splitEnv(
      "Toolbars[onlyNavBar,allToolbars],DevTools[bottomToolbox],Tabs"
    ),
    ["Toolbars[onlyNavBar,allToolbars]", "DevTools[bottomToolbox]", "Tabs"]
  );

  let filteredData = TestRunner.filterRestrictions("Toolbars[onlyNavBar]");
  equal(filteredData.trimmedSetName, "Toolbars");
  ok(filteredData.restrictions.has("onlyNavBar"));

  filteredData = TestRunner.filterRestrictions(
    "DevTools[bottomToolbox,sideToolbox]"
  );
  equal(filteredData.trimmedSetName, "DevTools");
  ok(filteredData.restrictions.has("bottomToolbox"));
  ok(filteredData.restrictions.has("sideToolbox"));
});
