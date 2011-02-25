function test()
{
  waitForExplicitFinish();

  test1();
}

/**
 * 1. load about:addons in a new tab and select that tab
 * 2. insert a button with tooltiptext
 * 3. create a new blank tab and select that tab
 * 4. select the about:addons tab and hover the inserted button
 * 5. remove the about:addons tab
 * 6. remove the blank tab
 *
 * the test succeeds if it doesn't trigger any assertions
 */
function test1() {
  let uri = "about:addons";
  let tab = gBrowser.addTab();

  tab.linkedBrowser.addEventListener("load", function(aEvent) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);

    let doc = gBrowser.contentDocument;
    var e = doc.createElement("button");
    e.setAttribute('label', "hello");
    e.setAttribute('tooltiptext', "world");
    doc.documentElement.insertBefore(e, doc.documentElement.firstChild);

    let tab2 = gBrowser.addTab();
    gBrowser.selectedTab = tab2;

    setTimeout(function() {
      gBrowser.selectedTab = tab;

      let doc = gBrowser.contentDocument;
      var win = gBrowser.contentWindow;
      EventUtils.disableNonTestMouseEvents(true);
      try {
        EventUtils.synthesizeMouse(e, 1, 1, { type: "mouseover" }, win);
        EventUtils.synthesizeMouse(e, 2, 6, { type: "mousemove" }, win);
        EventUtils.synthesizeMouse(e, 2, 4, { type: "mousemove" }, win);
      } finally {
        EventUtils.disableNonTestMouseEvents(false);
      }

      executeSoon(function() {
        gBrowser.removeTab(tab, {animate: false});
        gBrowser.removeTab(tab2, {animate: false});
        ok(true, "pass if no assertions");

        // done
        executeSoon(finish);
      });
    }, 0);
  }, true);

  gBrowser.selectedTab = tab;
  gBrowser.selectedTab.linkedBrowser.loadURI(uri);
}
