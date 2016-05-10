'use strict';

var pushNotifier = Cc['@mozilla.org/push/Notifier;1']
                     .getService(Ci.nsIPushNotifier);
var systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

function run_test() {
  run_next_test();
}

add_task(function* test_notifyWithData() {
  let textData = '{"hello":"world"}';
  let payload = new TextEncoder('utf-8').encode(textData);

  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData('chrome://notify-test', systemPrincipal,
    '' /* messageId */, payload.length, payload);

  let data = (yield notifyPromise).subject.QueryInterface(
    Ci.nsIPushMessage).data;
  deepEqual(data.json(), {
    hello: 'world',
  }, 'Should extract JSON values');
  deepEqual(data.binary(), Array.from(payload),
    'Should extract raw binary data');
  equal(data.text(), textData, 'Should extract text data');
});

add_task(function* test_empty_notifyWithData() {
  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData('chrome://notify-test', systemPrincipal,
    '' /* messageId */, 0, null);

  let data = (yield notifyPromise).subject.QueryInterface(
    Ci.nsIPushMessage).data;
  throws(_ => data.json(),
    'Should throw an error when parsing an empty string as JSON');
  strictEqual(data.text(), '', 'Should return an empty string');
  deepEqual(data.binary(), [], 'Should return an empty array');
});
