function promiseAlertWindow() {
  return new Promise(function(resolve) {
    let listener = {
      onOpenWindow(window) {
        let alertWindow = window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
        alertWindow.addEventListener("load", function onLoad() {
          alertWindow.removeEventListener("load", onLoad);
          let windowType = alertWindow.document.documentElement.getAttribute("windowtype");
          if (windowType != "alert:alert") {
            return;
          }
          Services.wm.removeListener(listener);
          resolve(alertWindow);
        });
      },
    };
    Services.wm.addListener(listener);
  });
}

/**
 * Similar to `BrowserTestUtils.closeWindow`, but
 * doesn't call `window.close()`.
 */
function promiseWindowClosed(window) {
  return new Promise(function(resolve) {
    Services.ww.registerNotification(function observer(subject, topic, data) {
      if (topic == "domwindowclosed" && subject == window) {
        Services.ww.unregisterNotification(observer);
        resolve();
      }
    });
  });
}
