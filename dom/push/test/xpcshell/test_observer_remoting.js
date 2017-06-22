'use strict';

const pushNotifier = Cc['@mozilla.org/push/Notifier;1']
                       .getService(Ci.nsIPushNotifier);

add_task(async function test_observer_remoting() {
  if (isParent) {
    await testInParent();
  } else {
    await testInChild();
  }
});

const childTests = [{
  text: 'Hello from child!',
  principal: Services.scriptSecurityManager.getSystemPrincipal(),
}];

const parentTests = [{
  text: 'Hello from parent!',
  principal: Services.scriptSecurityManager.getSystemPrincipal(),
}];

async function testInParent() {
  // Register observers for notifications from the child, then run the test in
  // the child and wait for the notifications.
  let promiseNotifications = childTests.reduce(
    (p, test) => p.then(_ => waitForNotifierObservers(test, /* shouldNotify = */ false)),
    Promise.resolve()
  );
  let promiseFinished = run_test_in_child('./test_observer_remoting.js');
  await promiseNotifications;

  // Wait until the child is listening for notifications from the parent.
  await do_await_remote_message('push_test_observer_remoting_child_ready');

  // Fire an observer notification in the parent that should be forwarded to
  // the child.
  await parentTests.reduce(
    (p, test) => p.then(_ => waitForNotifierObservers(test, /* shouldNotify = */ true)),
    Promise.resolve()
  );

  // Wait for the child to exit.
  await promiseFinished;
}

async function testInChild() {
  // Fire an observer notification in the child that should be forwarded to
  // the parent.
  await childTests.reduce(
    (p, test) => p.then(_ => waitForNotifierObservers(test, /* shouldNotify = */ true)),
    Promise.resolve()
  );

  // Register observers for notifications from the parent, let the parent know
  // we're ready, and wait for the notifications.
  let promiseNotifierObservers = parentTests.reduce(
    (p, test) => p.then(_ => waitForNotifierObservers(test, /* shouldNotify = */ false)),
    Promise.resolve()
  );
  do_send_remote_message('push_test_observer_remoting_child_ready');
  await promiseNotifierObservers;
}

var waitForNotifierObservers = async function({ text, principal }, shouldNotify = false) {
  let notifyPromise = promiseObserverNotification(
    PushServiceComponent.pushTopic);
  let subChangePromise = promiseObserverNotification(
    PushServiceComponent.subscriptionChangeTopic);
  let subModifiedPromise = promiseObserverNotification(
    PushServiceComponent.subscriptionModifiedTopic);

  let scope = 'chrome://test-scope';
  let data = new TextEncoder('utf-8').encode(text);

  if (shouldNotify) {
    pushNotifier.notifyPushWithData(scope, principal, '', data.length, data);
    pushNotifier.notifySubscriptionChange(scope, principal);
    pushNotifier.notifySubscriptionModified(scope, principal);
  }

  let {
    data: notifyScope,
    subject: notifySubject,
  } = await notifyPromise;
  equal(notifyScope, scope,
    'Should fire push notifications with the correct scope');
  let message = notifySubject.QueryInterface(Ci.nsIPushMessage);
  equal(message.principal, principal,
    'Should include the principal in the push message');
  strictEqual(message.data.text(), text, 'Should include data');

  let {
    data: subChangeScope,
    subject: subChangePrincipal,
  } = await subChangePromise;
  equal(subChangeScope, scope,
    'Should fire subscription change notifications with the correct scope');
  equal(subChangePrincipal, principal,
    'Should pass the principal as the subject of a change notification');

  let {
    data: subModifiedScope,
    subject: subModifiedPrincipal,
  } = await subModifiedPromise;
  equal(subModifiedScope, scope,
    'Should fire subscription modified notifications with the correct scope');
  equal(subModifiedPrincipal, principal,
    'Should pass the principal as the subject of a modified notification');
};
