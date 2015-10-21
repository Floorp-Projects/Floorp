/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");
let aboutNewTabService = Components.classes["@mozilla.org/browser/aboutnewtab-service;1"]
                                   .getService(Components.interfaces.nsIAboutNewTabService);

add_task(function* () {
  Assert.equal(aboutNewTabService.newTabURL, "about:newtab", "Default newtab URL should be about:newtab");
  let url = "http://example.com/";
  let notificationPromise = promiseNewtabURLNotification(url);
  aboutNewTabService.newTabURL = url;
  yield notificationPromise;
  Assert.ok(aboutNewTabService.overridden, "Newtab URL should be overridden");
  Assert.equal(aboutNewTabService.newTabURL, url, "Newtab URL should be the custom URL");

  notificationPromise = promiseNewtabURLNotification("about:newtab");
  aboutNewTabService.resetNewTabURL();
  yield notificationPromise;
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
  Assert.equal(aboutNewTabService.newTabURL, "about:newtab", "Newtab URL should be the about:newtab");

  // change newtab page to remote
  Services.prefs.setBoolPref("browser.newtabpage.remote", true);
  Assert.equal(aboutNewTabService.newTabURL, "about:remote-newtab", "Newtab URL should be the about:remote-newtab");
  Assert.ok(!aboutNewTabService.overridden, "Newtab URL should not be overridden");
});

function promiseNewtabURLNotification(aNewURL) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      Services.obs.removeObserver(observer, aTopic);
      Assert.equal(aData, aNewURL, "Data for newtab-url-changed notification should be new URL.");
      resolve();
    }, "newtab-url-changed", false);
  });
}
