/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let instance;
  let mgr = ResponsiveUI.ResponsiveUIManager;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,mop";

  function startTest() {
    mgr.once("on", function() {executeSoon(onUIOpen)});
    document.getElementById("Tools:ResponsiveUI").doCommand();
  }

  function onUIOpen() {
    instance = gBrowser.selectedTab.__responsiveUI;
    instance.stack.setAttribute("notransition", "true");
    ok(instance, "instance of the module is attached to the tab.");

    let mql = content.matchMedia("(max-device-width:100px)")

    ok(!mql.matches, "media query doesn't match.");

    mql.addListener(onMediaChange);
    instance.setSize(90, 500);
  }

  function onMediaChange(mql) {
    mql.removeListener(onMediaChange);
    ok(mql.matches, "media query matches.");
    ok(window.screen.width != content.screen.width, "screen.width is not the size of the screen.");
    is(content.screen.width, 90, "screen.width is the width of the page.");
    is(content.screen.height, 500, "screen.height is the height of the page.");


    let docShell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);

    mql.addListener(onMediaChange2);
    docShell.deviceSizeIsPageSize = false;
  }

  function onMediaChange2(mql) {
    mql.removeListener(onMediaChange);
    ok(!mql.matches, "media query has been re-evaluated.");
    ok(window.screen.width == content.screen.width, "screen.width is not the size of the screen.");
    instance.stack.removeAttribute("notransition");
    document.getElementById("Tools:ResponsiveUI").doCommand();
    gBrowser.removeCurrentTab();
    finish();
  }
}
