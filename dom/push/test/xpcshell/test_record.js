'use strict';

const {PushRecord} = Cu.import('resource://gre/modules/PushRecord.jsm', {});

function run_test() {
  run_next_test();
}

add_task(async function test_updateQuota() {
  let record = new PushRecord({
    quota: 8,
    lastPush: Date.now() - 1 * MS_IN_ONE_DAY,
  });

  record.updateQuota(Date.now() - 2 * MS_IN_ONE_DAY);
  equal(record.quota, 8,
    'Should not update quota if last visit is older than last push');

  record.updateQuota(Date.now());
  equal(record.quota, 16,
    'Should reset quota if last visit is newer than last push');

  record.reduceQuota();
  equal(record.quota, 15, 'Should reduce quota');

  // Make sure we calculate the quota correctly for visit dates in the
  // future (bug 1206424).
  record.updateQuota(Date.now() + 1 * MS_IN_ONE_DAY);
  equal(record.quota, 16,
    'Should reset quota to maximum if last visit is in the future');

  record.updateQuota(-1);
  strictEqual(record.quota, 0, 'Should set quota to 0 if history was cleared');
  ok(record.isExpired(), 'Should expire records once the quota reaches 0');
  record.reduceQuota();
  strictEqual(record.quota, 0, 'Quota should never be negative');
});

add_task(async function test_systemRecord_updateQuota() {
  let systemRecord = new PushRecord({
    quota: Infinity,
    systemRecord: true,
  });
  systemRecord.updateQuota(Date.now() - 3 * MS_IN_ONE_DAY);
  equal(systemRecord.quota, Infinity,
    'System subscriptions should ignore quota updates');
  systemRecord.updateQuota(-1);
  equal(systemRecord.quota, Infinity,
    'System subscriptions should ignore the last visit time');
  systemRecord.reduceQuota();
  equal(systemRecord.quota, Infinity,
    'System subscriptions should ignore quota reductions');
});

function testPermissionCheck(props) {
  let record = new PushRecord(props);
  equal(record.uri.spec, props.scope,
    `Record URI should match scope URL for ${JSON.stringify(props)}`);
  if (props.originAttributes) {
    let originSuffix = ChromeUtils.originAttributesToSuffix(
      record.principal.originAttributes);
    equal(originSuffix, props.originAttributes,
      `Origin suffixes should match for ${JSON.stringify(props)}`);
  }
  ok(!record.hasPermission(), `Record ${
    JSON.stringify(props)} should not have permission yet`);
  let permURI = Services.io.newURI(props.scope);
  Services.perms.add(permURI, 'desktop-notification',
                     Ci.nsIPermissionManager.ALLOW_ACTION);
  try {
    ok(record.hasPermission(), `Record ${
      JSON.stringify(props)} should have permission`);
  } finally {
    Services.perms.remove(permURI, 'desktop-notification');
  }
}

add_task(async function test_principal_permissions() {
  let testProps = [{
    scope: 'https://example.com/',
  }, {
    scope: 'https://example.com/',
    originAttributes: '^userContextId=1',
  }, {
    scope: 'https://блог.фанфрог.рф/',
  }, {
    scope: 'https://блог.фанфрог.рф/',
    originAttributes: '^userContextId=1',
  }];
  for (let props of testProps) {
    testPermissionCheck(props);
  }
});
