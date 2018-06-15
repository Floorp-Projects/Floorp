/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(async function test_unregister_empty_scope() {
  PushService.init({
    serverURI: "wss://push.example.org/",
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

  await rejects(
    PushService.unregister({
      scope: '',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    /Invalid page record/,
    'Expected error for empty endpoint'
  );
});
