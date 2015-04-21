/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_unregister_not_found() {
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: 'f074ed80-d479-44fa-ba65-792104a79ea9'
          }));
        }
      });
    }
  });

  let promise = PushNotificationService.unregister(
    'https://example.net/nonexistent');
  yield rejects(promise, function(error) {
    return error == 'NotFoundError';
  }, 'Wrong error for nonexistent scope');
});
