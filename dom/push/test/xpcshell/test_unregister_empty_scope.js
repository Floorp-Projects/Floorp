/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_unregister_empty_scope() {
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: '5619557c-86fe-4711-8078-d1fd6987aef7'
          }));
        }
      });
    }
  });

  yield rejects(
    PushNotificationService.unregister(''),
    function(error) {
      return error == 'NotFoundError';
    },
    'Wrong error for empty endpoint'
  );
});
