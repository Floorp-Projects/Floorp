"use strict";

var pushNotifier = Cc["@mozilla.org/push/Notifier;1"]
                     .getService(Ci.nsIPushNotifier);
var systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

add_task(async function test_notifyWithData() {
  let textData = '{"hello":"world"}';
  let payload = new TextEncoder("utf-8").encode(textData);

  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData("chrome://notify-test", systemPrincipal,
    "" /* messageId */, payload);

  let data = (await notifyPromise).subject.QueryInterface(
    Ci.nsIPushMessage).data;
  deepEqual(data.json(), {
    hello: "world",
  }, "Should extract JSON values");
  deepEqual(data.binary(), Array.from(payload),
    "Should extract raw binary data");
  equal(data.text(), textData, "Should extract text data");
});

add_task(async function test_empty_notifyWithData() {
  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData("chrome://notify-test", systemPrincipal,
    "" /* messageId */, []);

  let data = (await notifyPromise).subject.QueryInterface(
    Ci.nsIPushMessage).data;
  throws(_ => data.json(),
    /InvalidStateError/,
    "Should throw an error when parsing an empty string as JSON");
  strictEqual(data.text(), "", "Should return an empty string");
  deepEqual(data.binary(), [], "Should return an empty array");
});
