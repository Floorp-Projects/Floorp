var theTab;
var theBrowser;

function listener(evt) {
  if (evt.target == theBrowser.contentDocument) {
    doTest();
  }
}

function test() {
  waitForExplicitFinish();
  var testURL = getRootDirectory(gTestPath) + "newtab_share_rule_processors.html";
  theTab = gBrowser.addTab(testURL);
  theBrowser = gBrowser.getBrowserForTab(theTab);
  theBrowser.addEventListener("load", listener, true);
}

function doTest() {
  theBrowser.removeEventListener("load", listener, true);
  var winUtils = theBrowser.contentWindow
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);
  // The initial set of agent-level sheets should have a rule processor that's
  // also being used by another document.
  ok(winUtils.hasRuleProcessorUsedByMultipleStyleSets(Ci.nsIStyleSheetService.AGENT_SHEET),
     "agent sheet rule processor is used by multiple style sets");
  // Document-level sheets currently never get shared rule processors.
  ok(!winUtils.hasRuleProcessorUsedByMultipleStyleSets(Ci.nsIStyleSheetService.AUTHOR_SHEET),
     "author sheet rule processor is not used by multiple style sets");
  // Adding a unique style sheet to the agent level will cause it to have a
  // rule processor that is unique.
  theBrowser.contentWindow.wrappedJSObject.addAgentSheet();
  ok(!winUtils.hasRuleProcessorUsedByMultipleStyleSets(Ci.nsIStyleSheetService.AGENT_SHEET),
     "agent sheet rule processor is not used by multiple style sets after " +
     "having a unique sheet added to it");
  gBrowser.removeTab(theTab);
  finish();
}
