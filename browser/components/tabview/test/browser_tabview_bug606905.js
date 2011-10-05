/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let newTabs = []
  // add enough tabs so the close buttons are hidden and then check the closebuttons attribute
  do {
    let newTab = gBrowser.addTab("about:blank", {skipAnimation: true});
    newTabs.push(newTab);
  } while (gBrowser.visibleTabs[0].getBoundingClientRect().width > gBrowser.tabContainer.mTabClipWidth)

  // a setTimeout() in addTab is used to trigger adjustTabstrip() so we need a delay here as well.
  executeSoon(function() {
    is(gBrowser.tabContainer.getAttribute("closebuttons"), "activetab", "Only show button on selected tab.");

    // move a tab to another group and check the closebuttons attribute
    TabView._initFrame(function() {
      TabView.moveTabTo(newTabs[newTabs.length - 1], null);
      ok(gBrowser.visibleTabs[0].getBoundingClientRect().width > gBrowser.tabContainer.mTabClipWidth, 
         "Tab width is bigger than tab clip width");
      is(gBrowser.tabContainer.getAttribute("closebuttons"), "alltabs", "Show button on all tabs.")

      // clean up and finish
      newTabs.forEach(function(tab) {
        gBrowser.removeTab(tab);
      });
      finish();
    });
  });
}
