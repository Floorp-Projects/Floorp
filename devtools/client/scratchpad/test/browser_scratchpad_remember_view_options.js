/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 1140839 */

// Test that view menu items are remembered across scratchpad invocations.
function test() {
  waitForExplicitFinish();

  // To test for this bug we open a Scratchpad window and change all
  // view menu options. After each change we compare the correspondent
  // preference value with the expected value.

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html,<title>Bug 1140839</title>" +
                   "<p>test Scratchpad should remember View options");
}

function runTests() {
  const doc = gScratchpadWindow.document;

  const testData = [
    {itemMenuId: "sp-menu-line-numbers", prefId: "devtools.scratchpad.lineNumbers", expectedVal: false},
    {itemMenuId: "sp-menu-word-wrap", prefId: "devtools.scratchpad.wrapText", expectedVal: true},
    {itemMenuId: "sp-menu-highlight-trailing-space", prefId: "devtools.scratchpad.showTrailingSpace", expectedVal: true},
    {itemMenuId: "sp-menu-larger-font", prefId: "devtools.scratchpad.editorFontSize", expectedVal: 13},
    {itemMenuId: "sp-menu-normal-size-font", prefId: "devtools.scratchpad.editorFontSize", expectedVal: 12},
    {itemMenuId: "sp-menu-smaller-font", prefId: "devtools.scratchpad.editorFontSize", expectedVal: 11},
  ];

  testData.forEach(function(data) {
    const getPref = getPrefFunction(data.prefId);

    try {
      const menu = doc.getElementById(data.itemMenuId);
      menu.doCommand();
      const newPreferenceValue = getPref(data.prefId);
      ok(newPreferenceValue === data.expectedVal, newPreferenceValue + " !== " + data.expectedVal);
      Services.prefs.clearUserPref(data.prefId);
    } catch (exception) {
      ok(false, "Exception thrown while executing the command of menuitem #" + data.itemMenuId);
    }
  });

  finish();
}

function getPrefFunction(preferenceId) {
  const preferenceType = Services.prefs.getPrefType(preferenceId);
  if (preferenceType === Services.prefs.PREF_INT) {
    return Services.prefs.getIntPref;
  } else if (preferenceType === Services.prefs.PREF_BOOL) {
    return Services.prefs.getBoolPref;
  }
}
