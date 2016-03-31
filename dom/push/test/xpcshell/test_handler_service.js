"use strict";

// Here we test that if an xpcom component is registered with the category
// manager for push notifications against a specific scope, that service is
// instantiated before the message is delivered.

// This component is registered for "chrome://test-scope"
const kServiceContractID = "@mozilla.org/dom/push/test/PushServiceHandler;1";

let pushService = Cc["@mozilla.org/push/Service;1"].getService(Ci.nsIPushService);

add_test(function test_service_instantiation() {
  do_load_manifest("PushServiceHandler.manifest");

  let scope = "chrome://test-scope";
  let pushNotifier = Cc["@mozilla.org/push/Notifier;1"].getService(Ci.nsIPushNotifier);
  let principal = Services.scriptSecurityManager.getSystemPrincipal();
  pushNotifier.notifyPush(scope, principal, "");

  // Now get a handle to our service and check it received the notification.
  let handlerService = Cc[kServiceContractID]
                       .getService(Ci.nsISupports)
                       .wrappedJSObject;

  equal(handlerService.observed.length, 1);
  equal(handlerService.observed[0].topic, pushService.pushTopic);
  equal(handlerService.observed[0].data, scope);

  // and a subscription change.
  pushNotifier.notifySubscriptionChange(scope, principal);
  equal(handlerService.observed.length, 2);
  equal(handlerService.observed[1].topic, pushService.subscriptionChangeTopic);
  equal(handlerService.observed[1].data, scope);

  run_next_test();
});
