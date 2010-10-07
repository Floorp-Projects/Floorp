function test()
{
  waitForExplicitFinish();

  var level1 = false;
  var level2 = false;
  function test1() {
    // Load the following URI (which runs some child popup tests) in a new window (B),
    // then add a blank tab to B and call replaceTabWithWindow to detach the URI tab
    // into yet a new window (C), then close B.
    // Now run the tests again and then close C.
    // The test results does not matter, all this is just to exercise some code to
    // catch assertions or crashes.
    var chromeroot = getRootDirectory(gTestPath);
    var uri = chromeroot + "browser_tab_dragdrop2_frame1.xul";
    let window_B = openDialog(location, "_blank", "chrome,all,dialog=no,left=200,top=200,width=200,height=200", uri);
    window_B.addEventListener("load", function(aEvent) {
      window_B.removeEventListener("load", arguments.callee, false);
      if (level1) return; level1=true;
      executeSoon(function () {
        window_B.gBrowser.addEventListener("load", function(aEvent) {
          window_B.removeEventListener("load", arguments.callee, true);
          if (level2) return; level2=true;
          is(window_B.gBrowser.getBrowserForTab(window_B.gBrowser.tabs[0]).contentWindow.location, uri, "sanity check");
          //alert("1:"+window_B.gBrowser.getBrowserForTab(window_B.gBrowser.tabs[0]).contentWindow.location);
          var windowB_tab2 = window_B.gBrowser.addTab("about:blank", {skipAnimation: true});
          setTimeout(function () {
            //alert("2:"+window_B.gBrowser.getBrowserForTab(window_B.gBrowser.tabs[0]).contentWindow.location);
            window_B.gBrowser.addEventListener("pagehide", function(aEvent) {
              window_B.gBrowser.removeEventListener("pagehide", arguments.callee, true);
              executeSoon(function () {
                // alert("closing window_B which has "+ window_B.gBrowser.tabs.length+" tabs\n"+
                //      window_B.gBrowser.getBrowserForTab(window_B.gBrowser.tabs[0]).contentWindow.location);
                window_B.close();

                var doc = window_C.gBrowser.getBrowserForTab(window_C.gBrowser.tabs[0])
                            .docShell.contentViewer.DOMDocument.wrappedJSObject;
                var elems = document.documentElement.childNodes;
                var calls = doc.defaultView.test_panels();
                window_C.close();
                finish();
              });
            }, true);
            window_B.gBrowser.selectedTab = window_B.gBrowser.tabs[0];
            var window_C = window_B.gBrowser.replaceTabWithWindow(window_B.gBrowser.tabs[0]);
            }, 1000);  // 1 second to allow the tests to create the popups
        }, true);
      });
    }, false);
  }

  test1();
}
