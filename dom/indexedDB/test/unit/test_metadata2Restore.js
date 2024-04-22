/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* exported testGenerator */
var testGenerator = testSteps();

function* testSteps() {
  const openParams = [
    // This one lives in storage/default/http+++localhost+81^userContextId=1
    // The .metadata-v2 file was intentionally removed for this origin directory
    // to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:81",
      dbName: "dbC",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+82^userContextId=1
    // The .metadata-v2 file was intentionally truncated for this origin directory
    // to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:82",
      dbName: "dbD",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+83^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // 4 bytes of the 64 bit timestamp
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:83",
      dbName: "dbE",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+84^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:84",
      dbName: "dbF",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+85^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp and
    // the 8 bit persisted flag
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:85",
      dbName: "dbG",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+86^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag and
    // 2 bytes of the 32 bit reserved data 1
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:86",
      dbName: "dbH",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+87^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag and
    // the 32 bit reserved data 1
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:87",
      dbName: "dbI",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+88^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1 and
    // 2 bytes of the 32 bit reserved data 2
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:88",
      dbName: "dbJ",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+89^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1 and
    // the 32 bit reserved data 2
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:89",
      dbName: "dbK",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+90^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2 and
    // 2 bytes of the 32 bit suffix length
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:90",
      dbName: "dbL",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+91^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length and
    // first 5 chars of the suffix
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:91",
      dbName: "dbM",
      dbVersion: 1,
    },

    // This one lives in storage/default/http+++localhost+92^userContextId=1
    // The .metadata-v2 file was intentionally modified to contain only
    // the 64 bit timestamp,
    // the 8 bit persisted flag,
    // the 32 bit reserved data 1,
    // the 32 bit reserved data 2,
    // the 32 bit suffix length and
    // the suffix
    // for this origin directory to test restoring.
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:92",
      dbName: "dbN",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:93",
      dbName: "dbO",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:94",
      dbName: "dbP",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:95",
      dbName: "dbQ",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:96",
      dbName: "dbR",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:97",
      dbName: "dbS",
      dbVersion: 1,
    },

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
    {
      attrs: { userContextId: 1 },
      url: "http://localhost:98",
      dbName: "dbT",
      dbVersion: 1,
    },
  ];

  function openDatabase(params) {
    let request;
    if ("url" in params) {
      let uri = Services.io.newURI(params.url);
      let principal = Services.scriptSecurityManager.createContentPrincipal(
        uri,
        params.attrs || {}
      );
      request = indexedDB.openForPrincipal(
        principal,
        params.dbName,
        params.dbVersion
      );
    } else {
      request = indexedDB.open(params.dbName, params.dbVersion);
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
