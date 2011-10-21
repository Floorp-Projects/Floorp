/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// ----------
function test() {
  waitForExplicitFinish();

  let urlBase = "http://mochi.test:8888/browser/browser/components/tabview/test/";
  let newTab = gBrowser.addTab(urlBase + "search1.html");

  registerCleanupFunction(function() {
    if (gBrowser.tabs[1])
      gBrowser.removeTab(gBrowser.tabs[1]);
    TabView.hide();
  });

  afterAllTabsLoaded(function() {
    showTabView(function() {
      hideTabView(function() {
        newTab.linkedBrowser.loadURI(urlBase + "dummy_page.html");

        newWindowWithTabView(function(win) {
          registerCleanupFunction(function() win.close());

          let contentWindow = win.TabView.getContentWindow();

          EventUtils.synthesizeKey("d", { }, contentWindow);
          EventUtils.synthesizeKey("u", { }, contentWindow);
          EventUtils.synthesizeKey("m", { }, contentWindow);

          let resultsElement = contentWindow.document.getElementById("results");
          let childElements = resultsElement.childNodes;

          is(childElements.length, 1, "There is one result element");
          is(childElements[0].childNodes[1].textContent, 
             "This is a dummy test page", 
             "The label matches the title of dummy page");

          finish();
        });
      });
    });
  });
}

