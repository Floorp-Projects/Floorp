/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

// From browser/components/preferences/in-content/test/head.js
function open_preferences(aCallback) {
  gBrowser.selectedTab = gBrowser.addTab("about:preferences");
  let newTabBrowser = gBrowser.getBrowserForTab(gBrowser.selectedTab);
  newTabBrowser.addEventListener("Initialized", function () {
    newTabBrowser.removeEventListener("Initialized", arguments.callee, true);
    aCallback(gBrowser.contentWindow);
  }, true);
}

function test() {
  waitForExplicitFinish();
  let connectionTests = runConnectionTestsGen();
  connectionTests.next();
  const connectionURL = "chrome://browser/content/preferences/connection.xul";
  let closeable = false;
  let finalTest = false;
  let prefWin;

  // The changed preferences need to be backed up and restored because this mochitest
  // changes them setting from the default
  let oldNetworkProxyType = Services.prefs.getIntPref("network.proxy.type");
  registerCleanupFunction(function() {
    Services.prefs.setIntPref("network.proxy.type", oldNetworkProxyType);
    Services.prefs.clearUserPref("network.proxy.share_proxy_settings");
    for (let proxyType of ["http", "ssl", "ftp", "socks"]) {
      Services.prefs.clearUserPref("network.proxy." + proxyType);
      Services.prefs.clearUserPref("network.proxy." + proxyType + "_port");
      if (proxyType == "http") {
        continue;
      }
      Services.prefs.clearUserPref("network.proxy.backup." + proxyType);
      Services.prefs.clearUserPref("network.proxy.backup." + proxyType + "_port");
    }
    try {
      Services.ww.unregisterNotification(observer);
    } catch(e) {
      // Do nothing, if the test was successful the above line should fail silently.
    }
  });

  // this observer is registered after the pref tab loads
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic == "domwindowopened") {
        // when the connection window loads, proceed forward in test
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        win.addEventListener("load", function winLoadListener() {
          win.removeEventListener("load", winLoadListener);
          if (win.location.href == connectionURL) {
            // If this is a connection window, run the next test
            connectionTests.next(win);
          }
        });
      } else if (aTopic == "domwindowclosed") {
        // Check if the window should have closed, and respawn another window for further testing
        let win = aSubject.QueryInterface(Components.interfaces.nsIDOMWindow);
        if (win.location.href == connectionURL) {
          ok(closeable, "Connection dialog closed");

          // Last close event, don't respawn, and clean up
          if (finalTest) {
            Services.ww.unregisterNotification(observer);
            gBrowser.removeCurrentTab();
            finish();
            return;
          }

          // Open another connection pane for the next test
          gBrowser.contentWindow.gAdvancedPane.showConnections();
        }
      }
    }
  };

  // The actual tests to run, in a generator
  function* runConnectionTestsGen() {
    let doc, connectionWin, proxyTypePref, sharePref, httpPref, httpPortPref, ftpPref, ftpPortPref;

    // Convenient function to reset the variables for the new window
    function setDoc(win) {
      doc = win.document;
      connectionWin = win;
      proxyTypePref = doc.getElementById("network.proxy.type");
      sharePref = doc.getElementById("network.proxy.share_proxy_settings");
      httpPref = doc.getElementById("network.proxy.http");
      httpPortPref = doc.getElementById("network.proxy.http_port");
      ftpPref = doc.getElementById("network.proxy.ftp");
      ftpPortPref = doc.getElementById("network.proxy.ftp_port");
    }

    // This batch of tests should not close the dialog
    setDoc(yield null);

    // Testing HTTP port 0 with share on
    proxyTypePref.value = 1;
    sharePref.value = true;
    httpPref.value = "localhost";
    httpPortPref.value = 0;
    doc.documentElement.acceptDialog();

    // Testing HTTP port 0 + FTP port 80 with share off
    sharePref.value = false;
    ftpPref.value = "localhost";
    ftpPortPref.value = 80;
    doc.documentElement.acceptDialog();

    // Testing HTTP port 80 + FTP port 0 with share off
    httpPortPref.value = 80;
    ftpPortPref.value = 0;
    doc.documentElement.acceptDialog();

    // From now on, the dialog should close since we are giving it legitimate inputs.
    // The test will timeout if the onbeforeaccept kicks in erroneously.
    closeable = true;

    // Both ports 80, share on
    httpPortPref.value = 80;
    ftpPortPref.value = 80;
    doc.documentElement.acceptDialog();

    // HTTP 80, FTP 0, with share on
    setDoc(yield null);
    proxyTypePref.value = 1;
    sharePref.value = true;
    ftpPref.value = "localhost";
    httpPref.value = "localhost";
    httpPortPref.value = 80;
    ftpPortPref.value = 0;
    doc.documentElement.acceptDialog();

    // HTTP host empty, port 0 with share on
    setDoc(yield null);
    proxyTypePref.value = 1;
    sharePref.value = true;
    httpPref.value = "";
    httpPortPref.value = 0;
    doc.documentElement.acceptDialog();

    // HTTP 0, but in no proxy mode
    setDoc(yield null);
    proxyTypePref.value = 0;
    sharePref.value = true;
    httpPref.value = "localhost";
    httpPortPref.value = 0;

    // This is the final test, don't spawn another connection window
    finalTest = true;
    doc.documentElement.acceptDialog();
    yield null;
  }

  /*
   The connection dialog alone won't save onaccept since it uses type="child",
   so it has to be opened as a sub dialog of the main pref tab.
   Open the main tab here.
   */
  open_preferences(function tabOpened(aContentWindow) {
    Services.ww.registerNotification(observer);
    gBrowser.contentWindow.gAdvancedPane.showConnections();
  });
}
