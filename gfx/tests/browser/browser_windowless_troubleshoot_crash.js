let { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

add_task(function* test_windowlessBrowserTroubleshootCrash() {
  let webNav = Services.appShell.createWindowlessBrowser(false);

  let onLoaded = new Promise((resolve, reject) => {
    let docShell = webNav.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDocShell);
    let listener = {
      observe(contentWindow, topic, data) {
        let observedDocShell = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIWebNavigation)
                                            .QueryInterface(Ci.nsIDocShellTreeItem)
                                            .sameTypeRootTreeItem
                                            .QueryInterface(Ci.nsIDocShell);
          if (docShell === observedDocShell) {
            Services.obs.removeObserver(listener, "content-document-global-created", false);
            resolve();
          }
        }
    }
    Services.obs.addObserver(listener, "content-document-global-created", false);
  });
  webNav.loadURI("about:blank", 0, null, null, null);

  yield onLoaded;

  let winUtils = webNav.document.defaultView.
                        QueryInterface(Ci.nsIInterfaceRequestor).
                        getInterface(Ci.nsIDOMWindowUtils);
  is(winUtils.layerManagerType, "None", "windowless browser's layerManagerType should be 'None'");

  ok(true, "not crashed");

  var Troubleshoot = Cu.import("resource://gre/modules/Troubleshoot.jsm", {}).Troubleshoot;
  var data = yield new Promise((resolve, reject) => {
    Troubleshoot.snapshot((data) => {
      resolve(data);
    });
  });

  is(data.graphics.numTotalWindows, 2, "windowless browser window should not be counted as windows in the troubleshoot graphics report");
  is(data.graphics.numAcceleratedWindows, 0, "windowless browser window should not be counted as an accelerated window");
  is(data.graphics.windowLayerManagerType, "Basic", "windowless browser window should not set windowLayerManagerType to 'None'");
});
