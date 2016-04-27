'use strict';

var pushNotifier = Cc['@mozilla.org/push/Notifier;1']
                     .getService(Ci.nsIPushNotifier);
var systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();

function run_test() {
  run_next_test();
}

add_task(function* test_notifyWithData() {
  let textData = '{"hello":"world"}';
  let data = new TextEncoder('utf-8').encode(textData);

  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData('chrome://notify-test', systemPrincipal,
    '' /* messageId */, data.length, data);

  let message = (yield notifyPromise).subject.QueryInterface(Ci.nsIPushMessage);
  deepEqual(message.json(), {
    hello: 'world',
  }, 'Should extract JSON values');
  deepEqual(message.binary(), Array.from(data),
    'Should extract raw binary data');
  equal(message.text(), textData, 'Should extract text data');
});

add_task(function* test_empty_notifyWithData() {
  let notifyPromise =
    promiseObserverNotification(PushServiceComponent.pushTopic);
  pushNotifier.notifyPushWithData('chrome://notify-test', systemPrincipal,
    '' /* messageId */, 0, null);

  let message = (yield notifyPromise).subject.QueryInterface(Ci.nsIPushMessage);
  throws(_ => message.json(),
    'Should throw an error when parsing an empty string as JSON');
  strictEqual(message.text(), '', 'Should return an empty string');
  deepEqual(message.binary(), [], 'Should return an empty array');
});
