/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  let newTab = gBrowser.loadOneTab("http://www.example.com/#1",
                                   { inBackground: false });

  // The executeSoon() call is really needed here because there's callback
  // waiting to be fired after gBrowser.loadOneTab(). After that the browser is
  // in a state where loadURI() will create a new entry in the session history
  // (that is vital for back/forward functionality).
  afterAllTabsLoaded(function() {
    executeSoon(function() {
      ok(!newTab.linkedBrowser.canGoBack, 
         "Browser should not be able to go back in history");

      newTab.linkedBrowser.loadURI("http://www.example.com/#2");

      afterAllTabsLoaded(function() {
        ok(newTab.linkedBrowser.canGoBack, 
           "Browser can go back in history after loading another URL");

        showTabView(function() {
          let contentWindow = document.getElementById("tab-view").contentWindow;

          EventUtils.synthesizeKey("VK_BACK_SPACE", { type: "keyup" }, contentWindow);

          // check after a delay
          executeSoon(function() {
            ok(newTab.linkedBrowser.canGoBack, 
               "Browser can still go back in history");

            hideTabView(function() {
              gBrowser.removeTab(newTab);
              finish();
            });
          });
        });
      });
    });
  });
}

