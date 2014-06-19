// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let mockTab = function(aName, aIsClosing) {
  this.name = aName;
  this.isClosing = aIsClosing;
};

mockTab.prototype = {
  get chromeTab () {
    let getAttribute = () => this.isClosing;
    return {
      hasAttribute: getAttribute
    }
  }
};

let getMockBrowserTabs = function(aTestTabs, aSelected) {
  let result = {
    _tabs: aTestTabs,
    _selectedTab: aTestTabs[aSelected || 0],
    getTabAtIndex: function(i) this._tabs[i]
  };

  return [result, Browser.getNextTab.bind(result)];
};

gTests.push({
  desc: "Test Browser.getNextTab",
  run: function() {
    let [lonlyTab, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false)]);

    ok(fakeGetNextTab(lonlyTab._selectedTab) == null, "null when only 1 tab left");
    ok(fakeGetNextTab(new mockTab("x", false)) == null, "null when tab doesnt exist");

    let [twoTabs, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false), new mockTab("b", false)]);
    ok(fakeGetNextTab(twoTabs._selectedTab).name == "b", "get next tab");
    ok(fakeGetNextTab(twoTabs._tabs[1]).name == "a", "get selected tab");

    let [twoTabsSecondSelected, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false), new mockTab("b", false)], 1);
    ok(fakeGetNextTab(twoTabsSecondSelected._selectedTab).name == "a", "get previous tab");

    let [twoTabsOneClosing, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false), new mockTab("b", true)]);
    ok(fakeGetNextTab(twoTabsOneClosing._selectedTab) == null, "can't select closing tab");
    ok(fakeGetNextTab(twoTabsOneClosing._tabs[1]).name == "a", "get previous tab");

    let [twoTabsBothClosing, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", true), new mockTab("b", true)]);
    ok(fakeGetNextTab(twoTabsBothClosing._selectedTab) == null, "can't select closing tab");
    ok(fakeGetNextTab(twoTabsBothClosing._tabs[1]) == null, "can't select closing tab");

    let [fiveTabsLastNotClosing, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false), new mockTab("b", true), new mockTab("c", true), new mockTab("d", true), new mockTab("e", false)]);
    ok(fakeGetNextTab(fiveTabsLastNotClosing._selectedTab).name == "e", "get next non-closing tab");

    let [fiveTabsLastNotClosingLastSelected, fakeGetNextTab] = getMockBrowserTabs(
      [new mockTab("a", false), new mockTab("b", true), new mockTab("c", true), new mockTab("d", true), new mockTab("e", false)], 4);
    ok(fakeGetNextTab(fiveTabsLastNotClosingLastSelected._selectedTab).name == "a", "get previous non-closing tab");
  }
});

function test() {
  runTests();
}
