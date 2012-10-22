/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A mock notifications server.  Based on:
// dom/tests/mochitest/notification/notification_common.js
const FAKE_CID = Cc["@mozilla.org/uuid-generator;1"].
    getService(Ci.nsIUUIDGenerator).generateUUID();

const ALERTS_SERVICE_CONTRACT_ID = "@mozilla.org/alerts-service;1";
const ALERTS_SERVICE_CID = Components.ID(Cc[ALERTS_SERVICE_CONTRACT_ID].number);

function MockAlertsService() {}

MockAlertsService.prototype = {

    showAlertNotification: function(imageUrl, title, text, textClickable,
                                    cookie, alertListener, name) {
        let obData = JSON.stringify({
          imageUrl: imageUrl,
          title: title,
          text: text,
          textClickable: textClickable,
          cookie: cookie,
          name: name
        });
        Services.obs.notifyObservers(null, "social-test:notification-alert", obData);
    },

    QueryInterface: function(aIID) {
        if (aIID.equals(Ci.nsISupports) ||
            aIID.equals(Ci.nsIAlertsService))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    }
};

var factory = {
    createInstance: function(aOuter, aIID) {
        if (aOuter != null)
            throw Cr.NS_ERROR_NO_AGGREGATION;
        return new MockAlertsService().QueryInterface(aIID);
    }
};

function replacePromptService() {
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(FAKE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             factory)
}

function restorePromptService() {
  Components.manager.QueryInterface(Ci.nsIComponentRegistrar)
            .registerFactory(ALERTS_SERVICE_CID, "",
                             ALERTS_SERVICE_CONTRACT_ID,
                             null);
}
// end of alerts service mock.

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  Services.prefs.setBoolPref("social.debug.monitorUsage", true);
  Services.prefs.setIntPref("social.debug.monitorUsageTimeLimitMS", 1000);
  replacePromptService();
  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("social.debug.monitorUsage");
    Services.prefs.clearUserPref("social.debug.monitorUsageTimeLimitMS");
    restorePromptService();
  });

  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, finishcb);
  });
}

var tests = {
  testWorkerAPIAbuse: function(next) {
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    Services.obs.addObserver(function abuseObserver(subject, topic, data) {
      Services.obs.removeObserver(abuseObserver, "social-test:notification-alert");
      data = JSON.parse(data);
      is(data.title, "provider 1", "Abusive provider name should match");
      is(data.text,
         "Social API performance warning: More than 10 calls to social.cookies-get in less than 10 seconds.",
         "Usage warning should mention social.cookies-get");
      next();
    }, "social-test:notification-alert", false);

    for (let i = 0; i < 15; i++)
      port.postMessage({topic: "test-worker-spam-message"});
  },
  testTimeBetweenFirstAndLastMoreThanLimit: function(next) {
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    Services.obs.addObserver(function abuseObserver(subject, topic, data) {
      Services.obs.removeObserver(abuseObserver, "social-test:notification-alert");
      data = JSON.parse(data);
      is(data.title, "provider 1", "Abusive provider name should match");
      is(data.text,
         "Social API performance warning: More than 10 calls to social.cookies-get in less than 10 seconds.",
         "Usage warning should mention social.cookies-get");
      next();
    }, "social-test:notification-alert", false);

    port.postMessage({topic: "test-worker-spam-message"});
    setTimeout(function() {
      for (let i = 0; i < 15; i++)
        port.postMessage({topic: "test-worker-spam-message"});
    }, 2000);
  }
}
