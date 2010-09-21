function test()
{
  var embed = '<embed type="application/x-test" allowscriptaccess="always" allowfullscreen="true" wmode="window" width="640" height="480"></embed>'

  waitForExplicitFinish();

  // create a few tabs
  var tabs = [
    gBrowser.tabs[0],
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true}),
    gBrowser.addTab("about:blank", {skipAnimation: true})
  ];

  function setLocation(i, url) {
    gBrowser.getBrowserForTab(tabs[i]).contentWindow.location = url;
  }
  function moveTabTo(a, b) {
    gBrowser.swapBrowsersAndCloseOther(gBrowser.tabs[b], gBrowser.tabs[a]);
  }
  function clickTest(doc, win) {
    var clicks = doc.defaultView.clicks;
    EventUtils.synthesizeMouse(doc.body, 100, 600, {}, win);
    is(doc.defaultView.clicks, clicks+1, "adding 1 more click on BODY");
  }
  function test1() {
    moveTabTo(2, 3); // now: 0 1 2 4
    is(gBrowser.tabs[1], tabs[1], "tab1");
    is(gBrowser.tabs[2], tabs[3], "tab3");

    var plugin = gBrowser.getBrowserForTab(tabs[4]).docShell.contentViewer.DOMDocument.wrappedJSObject.body.firstChild;
    var tab4_plugin_object = plugin.getObjectValue();

    gBrowser.selectedTab = gBrowser.tabs[2];
    moveTabTo(3, 2); // now: 0 1 4
    gBrowser.selectedTab = tabs[4];
    var doc = gBrowser.getBrowserForTab(gBrowser.tabs[2]).docShell.contentViewer.DOMDocument.wrappedJSObject;
    plugin = doc.body.firstChild;
    ok(plugin && plugin.checkObjectValue(tab4_plugin_object), "same plugin instance");
    is(gBrowser.tabs[1], tabs[1], "tab1");
    is(gBrowser.tabs[2], tabs[3], "tab4");
    is(doc.defaultView.clicks, 0, "no click on BODY so far");
    clickTest(doc, window);

    moveTabTo(2, 1); // now: 0 4
    is(gBrowser.tabs[1], tabs[1], "tab1");
    doc = gBrowser.getBrowserForTab(gBrowser.tabs[1]).docShell.contentViewer.DOMDocument.wrappedJSObject;
    plugin = doc.body.firstChild;
    ok(plugin && plugin.checkObjectValue(tab4_plugin_object), "same plugin instance");
    clickTest(doc, window);

    // Load a new document (about:blank) in tab4, then detach that tab into a new window.
    // In the new window, navigate back to the original document and click on its <body>,
    // verify that its onclick was called.
    var t = tabs[1];
    var b = gBrowser.getBrowserForTab(t);
    gBrowser.selectedTab = t;
    b.addEventListener("load", function() {
      b.removeEventListener("load", arguments.callee, true);

      executeSoon(function () {
        var win = gBrowser.replaceTabWithWindow(t);
        win.addEventListener("load", function () {
          win.removeEventListener("load", arguments.callee, true);

          // Verify that the original window now only has the initial tab left in it.
          is(gBrowser.tabs[0], tabs[0], "tab0");
          is(gBrowser.getBrowserForTab(gBrowser.tabs[0]).contentWindow.location, "about:blank", "tab0 uri");

          executeSoon(function () {
            win.gBrowser.addEventListener("pageshow", function () {
              win.gBrowser.removeEventListener("pageshow", arguments.callee, false);
              executeSoon(function () {
                t = win.gBrowser.tabs[0];
                b = win.gBrowser.getBrowserForTab(t);
                var doc = b.docShell.contentViewer.DOMDocument.wrappedJSObject;
                clickTest(doc, win);
                win.close();
                finish();
              });
            }, false);
            win.gBrowser.goBack();
          });
        }, true);
      });
    }, true);
    b.loadURI("about:blank");

  }

  var loads = 0;
  function waitForLoad(tab) {
    gBrowser.getBrowserForTab(gBrowser.tabs[tab]).removeEventListener("load", arguments.callee, true);
    ++loads;
    if (loads == tabs.length - 1) {
      executeSoon(test1);
    }
  }

  function fn(f, arg) {
    return function () { return f(arg); };
  }
  for (var i = 1; i < tabs.length; ++i) {
    gBrowser.getBrowserForTab(tabs[i]).addEventListener("load", fn(waitForLoad,i), true);
  }

  setLocation(1, "data:text/html,<title>tab1</title><body>tab1<iframe>");
  setLocation(2, "data:text/plain,tab2");
  setLocation(3, "data:text/html,<title>tab3</title><body>tab3<iframe>");
  setLocation(4, "data:text/html,<body onload='clicks=0' onclick='++clicks'>"+embed);
  gBrowser.selectedTab = tabs[3];

}
