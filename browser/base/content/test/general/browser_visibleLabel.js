/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests:
 *   verify that the visibleLabel attribute works
 *   verify the TabLabelModified event works for both existing and new tabs
 */

function test() {
  waitForExplicitFinish();
  registerCleanupFunction(function() {
    gBrowser.removeCurrentTab({animate: false});
  });
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank",
                                                   {skipAnimation: true});
  tab.linkedBrowser.addEventListener("load", function onLoad(event) {
    event.currentTarget.removeEventListener("load", onLoad, true);
    executeSoon(afterLoad);
  }, true);
}

function afterLoad() {
  let tab = gBrowser.selectedTab;
  let xulLabel = document.getAnonymousElementByAttribute(tab, "anonid",
                                                         "tab-label");
  // Verify we're starting out on the right foot
  is(tab.label, "New Tab", "Initial tab label is default");
  is(xulLabel.value, "New Tab", "Label element is default");
  is(tab.visibleLabel, "New Tab", "visibleLabel is default");

  // Check that a normal label setting works correctly
  tab.label = "Hello, world!";
  is(tab.label, "Hello, world!", "tab label attribute set via tab.label");
  is(xulLabel.value, "Hello, world!", "xul:label set via tab.label");
  is(tab.visibleLabel, "Hello, world!", "visibleLabel set via tab.label");

  // Check that setting visibleLabel only affects the label element
  tab.visibleLabel = "Goodnight, Irene";
  is(tab.label, "Hello, world!", "Tab.label unaffected by visibleLabel setter");
  is(xulLabel.value, "Goodnight, Irene",
     "xul:label set by visibleLabel setter");
  is(tab.visibleLabel, "Goodnight, Irene",
     "visibleLabel attribute set by visibleLabel setter");

  // Check that setting the label property hits everything
  tab.label = "One more label";
  is(tab.label, "One more label",
     "Tab label set via label property after diverging from visibleLabel");
  is(xulLabel.value, "One more label",
     "xul:label set via label property after diverging from visibleLabel");
  is(tab.visibleLabel, "One more label",
     "visibleLabel set from label property after diverging from visibleLabel");

  tab.addEventListener("TabLabelModified", overrideTabLabel, true);
  tab.label = "This won't be the visibleLabel";
}

function overrideTabLabel(aEvent) {
  aEvent.target.removeEventListener("TabLabelModified", overrideTabLabel, true);
  aEvent.preventDefault();
  aEvent.stopPropagation();
  aEvent.target.visibleLabel = "Handler set this as the visible label";
  executeSoon(checkTabLabelModified);
}

function checkTabLabelModified() {
  let tab = gBrowser.selectedTab;
  let xulLabel = document.getAnonymousElementByAttribute(tab, "anonid",
                                                         "tab-label");

  is(tab.label, "This won't be the visibleLabel",
     "Tab label set via label property that triggered event");
  is(xulLabel.value, "Handler set this as the visible label",
     "xul:label set by TabLabelModified handler");
  is(tab.visibleLabel, "Handler set this as the visible label",
     "visibleLabel set by TabLabelModified handler");

  gBrowser.removeCurrentTab({animate: false});
  executeSoon(checkTabLabelModifiedOnNewTab);
}

function checkTabLabelModifiedOnNewTab() {
  gBrowser.tabContainer.addEventListener("TabLabelModified",
    handleTabLabelModifiedOnNewTab, true);
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank",
                                                   {skipAnimation: true});
}

function handleTabLabelModifiedOnNewTab(aEvent) {
  gBrowser.tabContainer.removeEventListener("TabLabelModified",
    handleTabLabelModifiedOnNewTab, true);
  ok(true, "Event received from new tab default being set");
  executeSoon(finish);
}
