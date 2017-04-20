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
  return ContentTask.spawn(aBrowser, [fn, timeout], function* ([contentFn, contentTimeout]) {
    let win = content.wrappedJSObject;
    let notification = win[contentFn]();
    win._notification = notification;
    yield new Promise((resolve, reject) => {
      function listener() {
        notification.removeEventListener("show", listener);
        resolve();
      }

      notification.addEventListener("show", listener);

      if (contentTimeout) {
        content.setTimeout(() => {
          notification.removeEventListener("show", listener);
          reject("timed out");
        }, contentTimeout);
      }
    });
  });
}

function closeNotification(aBrowser) {
  return ContentTask.spawn(aBrowser, null, function() {
    content.wrappedJSObject._notification.close();
  });
}
