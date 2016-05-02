'use strict';

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(function* test_observer_notifications() {
  // Push observer notifications dispatched in the child should be forwarded to
  // the parent.
  let notifyPromise = promiseObserverNotification(
    PushServiceComponent.pushTopic);
  let subChangePromise = promiseObserverNotification(
    PushServiceComponent.subscriptionChangeTopic);
  let subModifiedPromise = promiseObserverNotification(
    PushServiceComponent.subscriptionModifiedTopic);

  yield run_test_in_child('./test_handler_service.js');

  let {data: notifyScope} = yield notifyPromise;
  equal(notifyScope, 'chrome://test-scope',
    'Should forward push notifications with the correct scope');

  let {data: subChangeScope} = yield subChangePromise;
  equal(subChangeScope, 'chrome://test-scope',
    'Should forward subscription change notifications with the correct scope');

  let {data: subModifiedScope} = yield subModifiedPromise;
  equal(subModifiedScope, 'chrome://test-scope',
    'Should forward subscription modified notifications with the correct scope');
});
