"use strict";

// Here we test that if an xpcom component is registered with the category
// manager for push notifications against a specific scope, that service is
// instantiated before the message is delivered.

const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);

let pushService = Cc["@mozilla.org/push/Service;1"].getService(
  Ci.nsIPushService
);

function PushServiceHandler() {
  // Register a push observer.
  this.observed = [];
  Services.obs.addObserver(this, pushService.pushTopic);
  Services.obs.addObserver(this, pushService.subscriptionChangeTopic);
  Services.obs.addObserver(this, pushService.subscriptionModifiedTopic);
}

PushServiceHandler.prototype = {
  classID: Components.ID("{bb7c5199-c0f7-4976-9f6d-1306e32c5591}"),
  QueryInterface: ChromeUtils.generateQI([]),

  observe(subject, topic, data) {
    this.observed.push({ subject, topic, data });
  },
};

let handlerService = new PushServiceHandler();

add_test(function test_service_instantiation() {
  const CONTRACT_ID = "@mozilla.org/dom/push/test/PushServiceHandler;1";
  let scope = "chrome://test-scope";

  MockRegistrar.register(CONTRACT_ID, handlerService);
  Services.catMan.addCategoryEntry("push", scope, CONTRACT_ID, false, false);

  let pushNotifier = Cc["@mozilla.org/push/Notifier;1"].getService(
    Ci.nsIPushNotifier
  );
  let principal = Services.scriptSecurityManager.getSystemPrincipal();
  pushNotifier.notifyPush(scope, principal, "");

  equal(handlerService.observed.length, 1);
  equal(handlerService.observed[0].topic, pushService.pushTopic);
  let message = handlerService.observed[0].subject.QueryInterface(
    Ci.nsIPushMessage
  );
  equal(message.principal, principal);
  strictEqual(message.data, null);
  equal(handlerService.observed[0].data, scope);

  // and a subscription change.
  pushNotifier.notifySubscriptionChange(scope, principal);
  equal(handlerService.observed.length, 2);
  equal(handlerService.observed[1].topic, pushService.subscriptionChangeTopic);
  equal(handlerService.observed[1].subject, principal);
  equal(handlerService.observed[1].data, scope);

  // and a subscription modified event.
  pushNotifier.notifySubscriptionModified(scope, principal);
  equal(handlerService.observed.length, 3);
  equal(
    handlerService.observed[2].topic,
    pushService.subscriptionModifiedTopic
  );
  equal(handlerService.observed[2].subject, principal);
  equal(handlerService.observed[2].data, scope);

  run_next_test();
});
