/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = '997ee7ba-36b1-4526-ae9e-2d3f38d6efe8';

function run_test() {
  do_get_profile();
  setPrefs({userAgentID});
  run_next_test();
}

add_task(function* test_registration_success() {
  let db = new PushDB();
  let promiseDB = promisifyDatabase(db);
  do_register_cleanup(() => cleanupDatabase(db));
  let records = [{
    channelID: 'bf001fe0-2684-42f2-bc4d-a3e14b11dd5b',
    pushEndpoint: 'https://example.com/update/same-manifest/1',
    scope: 'https://example.net/a',
    version: 5
  }, {
    channelID: 'f6edfbcd-79d6-49b8-9766-48b9dcfeff0f',
    pushEndpoint: 'https://example.com/update/same-manifest/2',
    scope: 'https://example.net/b',
    version: 10
  }, {
    channelID: 'b1cf38c9-6836-4d29-8a30-a3e98d59b728',
    pushEndpoint: 'https://example.org/update/different-manifest',
    scope: 'https://example.org/c',
    version: 15
  }];
  for (let record of records) {
    yield promiseDB.put(record);
  }

  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          equal(request.uaid, userAgentID, 'Wrong device ID in handshake');
          deepEqual(request.channelIDs.sort(), [
            'b1cf38c9-6836-4d29-8a30-a3e98d59b728',
            'bf001fe0-2684-42f2-bc4d-a3e14b11dd5b',
            'f6edfbcd-79d6-49b8-9766-48b9dcfeff0f',
          ], 'Wrong channel list in handshake');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
        }
      });
    }
  });

  let registration = yield PushNotificationService.registration(
    'https://example.net/a');
  deepEqual(registration, {
    pushEndpoint: 'https://example.com/update/same-manifest/1',
    version: 5
  }, 'Should include registrations for all pages with this manifest');
});
