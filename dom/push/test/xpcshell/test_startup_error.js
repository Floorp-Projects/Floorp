'use strict';

const {PushService, PushServiceWebSocket} = serviceExports;

function run_test() {
  setPrefs();
  do_get_profile();
  run_next_test();
}

add_task(function* test_startup_error() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: 'wss://push.example.org/',
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getAllExpired(prev) {
        return Promise.reject('database corruption on startup');
      },
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          ok(false, 'Unexpected handshake');
        },
        onRegister(request) {
          ok(false, 'Unexpected register request');
        },
      });
    },
  });

  yield rejects(
    PushService.register({
      scope: `https://example.net/1`,
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Should not register if startup failed'
  );

  PushService.uninit();

  PushService.init({
    serverURI: 'wss://push.example.org/',
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getAllUnexpired(prev) {
        return Promise.reject('database corruption on connect');
      },
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          ok(false, 'Unexpected handshake');
        },
        onRegister(request) {
          ok(false, 'Unexpected register request');
        },
      });
    },
  });
  yield rejects(
    PushService.registration({
      scope: `https://example.net/1`,
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Should not return registration if connection failed'
  );
});
