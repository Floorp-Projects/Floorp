/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.importGlobalProperties(["crypto"]);

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

var db;

function done() {
  do_test_finished();
  run_next_test();
}

function generateKey() {
  return crypto.subtle.generateKey({
    name: "ECDSA",
    namedCurve: "P-256",
  }, true, ["sign", "verify"]).then(cryptoKey =>
    crypto.subtle.exportKey("raw", cryptoKey.publicKey)
  ).then(publicKey => new Uint8Array(publicKey));
}

function run_test() {
  if (isParent) {
    do_get_profile();
  }
  run_next_test();
}

if (isParent) {
  add_test(function setUp() {
    db = PushServiceWebSocket.newPushDB();
    do_register_cleanup(() => {return db.drop().then(_ => db.close());});
    setUpServiceInParent(PushService, db).then(run_next_test, run_next_test);
  });
}

add_test(function test_subscribe_success() {
  do_test_pending();
  PushServiceComponent.subscribe(
    'https://example.com/sub/ok',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, subscription) => {
      ok(Components.isSuccessCode(result), 'Error creating subscription');
      ok(subscription.isSystemSubscription, 'Expected system subscription');
      ok(subscription.endpoint.startsWith('https://example.org/push'), 'Wrong endpoint prefix');
      equal(subscription.pushCount, 0, 'Wrong push count');
      equal(subscription.lastPush, 0, 'Wrong last push time');
      equal(subscription.quota, -1, 'Wrong quota for system subscription');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_subscribeWithKey_error() {
  do_test_pending();

  let invalidKey = [0, 1];
  PushServiceComponent.subscribeWithKey(
    'https://example.com/sub-key/invalid',
    Services.scriptSecurityManager.getSystemPrincipal(),
    invalidKey.length,
    invalidKey,
    (result, subscription) => {
      ok(!Components.isSuccessCode(result), 'Expected error creating subscription with invalid key');
      equal(result, Cr.NS_ERROR_DOM_PUSH_INVALID_KEY_ERR, 'Wrong error code for invalid key');
      strictEqual(subscription, null, 'Unexpected subscription');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_subscribeWithKey_success() {
  do_test_pending();

  generateKey().then(key => {
    PushServiceComponent.subscribeWithKey(
      'https://example.com/sub-key/ok',
      Services.scriptSecurityManager.getSystemPrincipal(),
      key.length,
      key,
      (result, subscription) => {
        ok(Components.isSuccessCode(result), 'Error creating subscription with key');
        notStrictEqual(subscription, null, 'Expected subscription');
        done();
      }
    );
  }, error => {
    ok(false, "Error generating app server key");
    done();
  });
});

add_test(function test_subscribeWithKey_conflict() {
  do_test_pending();

  generateKey().then(differentKey => {
    PushServiceComponent.subscribeWithKey(
      'https://example.com/sub-key/ok',
      Services.scriptSecurityManager.getSystemPrincipal(),
      differentKey.length,
      differentKey,
      (result, subscription) => {
        ok(!Components.isSuccessCode(result), 'Expected error creating subscription with conflicting key');
        equal(result, Cr.NS_ERROR_DOM_PUSH_MISMATCHED_KEY_ERR, 'Wrong error code for mismatched key');
        strictEqual(subscription, null, 'Unexpected subscription');
        done();
      }
    );
  }, error => {
    ok(false, "Error generating different app server key");
    done();
  });
});

add_test(function test_subscribe_error() {
  do_test_pending();
  PushServiceComponent.subscribe(
    'https://example.com/sub/fail',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, subscription) => {
      ok(!Components.isSuccessCode(result), 'Expected error creating subscription');
      strictEqual(subscription, null, 'Unexpected subscription');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_getSubscription_exists() {
  do_test_pending();
  PushServiceComponent.getSubscription(
    'https://example.com/get/ok',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, subscription) => {
      ok(Components.isSuccessCode(result), 'Error getting subscription');

      equal(subscription.endpoint, 'https://example.org/push/get', 'Wrong endpoint');
      equal(subscription.pushCount, 10, 'Wrong push count');
      equal(subscription.lastPush, 1438360548322, 'Wrong last push');
      equal(subscription.quota, 16, 'Wrong quota for subscription');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_getSubscription_missing() {
  do_test_pending();
  PushServiceComponent.getSubscription(
    'https://example.com/get/missing',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, subscription) => {
      ok(Components.isSuccessCode(result), 'Error getting nonexistent subscription');
      strictEqual(subscription, null, 'Nonexistent subscriptions should return null');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_getSubscription_error() {
  do_test_pending();
  PushServiceComponent.getSubscription(
    'https://example.com/get/fail',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, subscription) => {
      ok(!Components.isSuccessCode(result), 'Expected error getting subscription');
      strictEqual(subscription, null, 'Unexpected subscription');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_unsubscribe_success() {
  do_test_pending();
  PushServiceComponent.unsubscribe(
    'https://example.com/unsub/ok',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, success) => {
      ok(Components.isSuccessCode(result), 'Error unsubscribing');
      strictEqual(success, true, 'Expected successful unsubscribe');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_unsubscribe_nonexistent() {
  do_test_pending();
  PushServiceComponent.unsubscribe(
    'https://example.com/unsub/ok',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, success) => {
      ok(Components.isSuccessCode(result), 'Error removing nonexistent subscription');
      strictEqual(success, false, 'Nonexistent subscriptions should return false');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_unsubscribe_error() {
  do_test_pending();
  PushServiceComponent.unsubscribe(
    'https://example.com/unsub/fail',
    Services.scriptSecurityManager.getSystemPrincipal(),
    (result, success) => {
      ok(!Components.isSuccessCode(result), 'Expected error unsubscribing');
      strictEqual(success, false, 'Unexpected successful unsubscribe');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_subscribe_app_principal() {
  let principal = Services.scriptSecurityManager.getAppCodebasePrincipal(
    Services.io.newURI('https://example.net/app/1'),
    1, /* appId */
    true /* browserOnly */
  );

  do_test_pending();
  PushServiceComponent.subscribe('https://example.net/scope/1', principal, (result, subscription) => {
    ok(Components.isSuccessCode(result), 'Error creating subscription');
    ok(subscription.endpoint.startsWith('https://example.org/push'),
      'Wrong push endpoint in app subscription');
    ok(!subscription.isSystemSubscription,
      'Unexpected system subscription for app principal');
    equal(subscription.quota, 16, 'Wrong quota for app subscription');

    do_test_finished();
    run_next_test();
  });
});

add_test(function test_subscribe_origin_principal() {
  let scope = 'https://example.net/origin-principal';
  let principal =
    Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(scope);

  do_test_pending();
  PushServiceComponent.subscribe(scope, principal, (result, subscription) => {
    ok(Components.isSuccessCode(result),
      'Expected error creating subscription with origin principal');
    ok(!subscription.isSystemSubscription,
      'Unexpected system subscription for origin principal');
    equal(subscription.quota, 16, 'Wrong quota for origin subscription');

    do_test_finished();
    run_next_test();
  });
});

add_test(function test_subscribe_null_principal() {
  do_test_pending();
  PushServiceComponent.subscribe(
    'chrome://push/null-principal',
    Services.scriptSecurityManager.createNullPrincipal({}),
    (result, subscription) => {
      ok(!Components.isSuccessCode(result),
        'Expected error creating subscription with null principal');
      strictEqual(subscription, null,
        'Unexpected subscription with null principal');

      do_test_finished();
      run_next_test();
    }
  );
});

add_test(function test_subscribe_missing_principal() {
  do_test_pending();
  PushServiceComponent.subscribe('chrome://push/missing-principal', null,
    (result, subscription) => {
      ok(!Components.isSuccessCode(result),
        'Expected error creating subscription without principal');
      strictEqual(subscription, null,
        'Unexpected subscription without principal');

      do_test_finished();
      run_next_test();
    }
  );
});

if (isParent) {
  add_test(function tearDown() {
    tearDownServiceInParent(db).then(run_next_test, run_next_test);
  });
}
