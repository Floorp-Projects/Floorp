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

/**
 * These two functions work with file_dom_notifications.html to open the
 * notification and close it.
 *
 * |fn| can be showNotification1 or showNotification2.
 * if |timeout| is passed, then the promise returned from this function is
 * rejected after the requested number of miliseconds.
 */
function openNotification(aBrowser, fn, timeout) {
  return ContentTask.spawn(aBrowser, { fn, timeout }, function* ({ fn, timeout }) {
    let win = content.wrappedJSObject;
    let notification = win[fn]();
    win._notification = notification;
    yield new Promise((resolve, reject) => {
      function listener() {
        notification.removeEventListener("show", listener);
        resolve();
      }

      notification.addEventListener("show", listener);

      if (timeout) {
        content.setTimeout(() => {
          notification.removeEventListener("show", listener);
          reject("timed out");
        }, timeout);
      }
    });
  });
}

function closeNotification(aBrowser) {
  return ContentTask.spawn(aBrowser, null, function() {
    content.wrappedJSObject._notification.close();
  });
}
