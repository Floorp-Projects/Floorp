/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const openParams = [
    // This one lives in storage/permanent/chrome
    // The .metadata-v2 file was intentionally removed for this origin directory
    // to test restoring.
    { dbName: "dbA",
      dbOptions: { version: 1, storage: "persistent" } },

    // This one lives in storage/temporary/http+++localhost
    // The .metadata-v2 file was intentionally removed for this origin directory
    // to test restoring.
    { url: "http://localhost", dbName: "dbB",
      dbOptions: { version: 1, storage: "temporary" } },

    // This one lives in storage/default/http+++localhost+81^userContextId=1
    // The .metadata-v2 file was intentionally removed for this origin directory
    // to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:81", dbName: "dbC",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+82^userContextId=1
    // The .metadata-v2 file was intentionally truncated for this origin directory
    // to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:82", dbName: "dbD",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+83^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // 4 bytes of the 64 bit timestamp
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:83", dbName: "dbE",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+84^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:84", dbName: "dbF",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+85^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp and
    // the 8 bit persisted flag
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:85", dbName: "dbG",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+86^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag and
    // 2 bytes of the 32 bit reserved data 1
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:86", dbName: "dbH",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+87^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag and
    // the 32 bit reserved data 1
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:87", dbName: "dbI",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+88^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1 and
    // 2 bytes of the 32 bit reserved data 2
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:88", dbName: "dbJ",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+89^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1 and
    // the 32 bit reserved data 2
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:89", dbName: "dbK",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+90^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2 and
    // 2 bytes of the 32 bit suffix length
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:90", dbName: "dbL",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+91^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length and
    // first 5 chars of the suffix
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:91", dbName: "dbM",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+92^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length and
    // the suffix
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:92", dbName: "dbN",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+93^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix and
    // 2 bytes of the 32 bit group length
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:93", dbName: "dbO",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+94^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix,
    // the 32 bit group length and
    // first 5 chars of the group
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:94", dbName: "dbP",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+95^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix,
    // the 32 bit group length and
    // the group
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:95", dbName: "dbQ",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+96^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix,
    // the 32 bit group length,
    // the group and
    // 2 bytes of the 32 bit origin length
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:96", dbName: "dbR",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+97^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix,
    // the 32 bit group length,
    // the group,
    // the 32 bit origin length and
    // first 12 char of the origin
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:97", dbName: "dbS",
      dbOptions: { version: 1, storage: "default" } },

    // This one lives in storage/default/http+++localhost+98^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length,
    // the suffix,
    // the 32 bit group length,
    // the group,
    // the 32 bit origin length and
    // the origin
    // for this origin directory to test restoring.
    { attrs: { userContextId: 1 }, url: "http://localhost:98", dbName: "dbT",
      dbOptions: { version: 1, storage: "default" } }
  ];

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let uri = Services.io.newURI(params.url);
      let principal = Services.scriptSecurityManager
        .createCodebasePrincipal(uri, params.attrs || {});
      request = indexedDB.openForPrincipal(principal, params.dbName,
                                           params.dbOptions);
    } else {
      request = indexedDB.open(params.dbName, params.dbOptions);
    }
    return request;
  }

  clearAllDatabases(continueToNextStepSync);
  yield undefined;

  installPackagedProfile("metadata2Restore_profile");

  for (let params of openParams) {
    let request = openDatabase(params);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Correct event type");
  }

  resetAllDatabases(continueToNextStepSync);
  yield undefined;

  for (let params of openParams) {
    let request = openDatabase(params);
    request.onerror = errorHandler;
    request.onupgradeneeded = unexpectedSuccessHandler;
    request.onsuccess = grabEventAndContinueHandler;
    let event = yield undefined;

    is(event.type, "success", "Correct event type");
  }

  finishTest();
  yield undefined;
}
