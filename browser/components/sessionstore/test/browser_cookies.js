"use strict";

function promiseSetCookie(cookie) {
  info(`Set-Cookie: ${cookie}`);
  return Promise.all([
    waitForCookieChanged(),
    ContentTask.spawn(gBrowser.selectedBrowser, cookie,
      passedCookie => content.document.cookie = passedCookie)
  ]);
}

function waitForCookieChanged() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subj, topic, data) {
      Services.obs.removeObserver(observer, topic);
      resolve();
    }, "cookie-changed", false);
  });
}

function cookieExists(host, name, value) {
  let {cookies: [c]} = JSON.parse(ss.getBrowserState());
  return c && c.host == host && c.name == name && c.value == value;
}

// Setup and cleanup.
add_task(function* test_setup() {
  registerCleanupFunction(() => {
    Services.cookies.removeAll();
  });
});

// Test session cookie storage.
add_task(function* test_run() {
  Services.cookies.removeAll();

  // Add a new tab for testing.
  gBrowser.selectedTab = gBrowser.addTab("http://example.com/");
  yield promiseBrowserLoaded(gBrowser.selectedBrowser);

  // Add a session cookie.
  yield promiseSetCookie("foo=bar");
  ok(cookieExists("example.com", "foo", "bar"), "cookie was added");

  // Modify a session cookie.
  yield promiseSetCookie("foo=baz");
  ok(cookieExists("example.com", "foo", "baz"), "cookie was modified");

  // Turn the session cookie into a normal one.
  let expiry = new Date();
  expiry.setDate(expiry.getDate() + 2);
  yield promiseSetCookie(`foo=baz; Expires=${expiry.toUTCString()}`);
  ok(!cookieExists("example.com", "foo", "baz"), "no longer a session cookie");

  // Turn it back into a session cookie.
  yield promiseSetCookie("foo=bar");
  ok(cookieExists("example.com", "foo", "bar"), "again a session cookie");

  // Remove the session cookie.
  yield promiseSetCookie("foo=; Expires=Thu, 01 Jan 1970 00:00:00 GMT");
  ok(!cookieExists("example.com", "foo", ""), "cookie was removed");

  // Add a session cookie.
  yield promiseSetCookie("foo=bar");
  ok(cookieExists("example.com", "foo", "bar"), "cookie was added");

  // Clear all session cookies.
  Services.cookies.removeAll();
  ok(!cookieExists("example.com", "foo", "bar"), "cookies were cleared");

  // Cleanup.
  yield promiseRemoveTab(gBrowser.selectedTab);
});
