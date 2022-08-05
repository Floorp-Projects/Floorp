/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

const TemporaryPermissions = SitePermissions._temporaryPermissions;

const PERM_A = "foo";
const PERM_B = "bar";
const PERM_C = "foobar";

const BROWSER_A = createDummyBrowser("https://example.com/foo");
const BROWSER_B = createDummyBrowser("https://example.org/foo");

const EXPIRY_MS_A = 1000000;
const EXPIRY_MS_B = 1000001;

function createDummyBrowser(spec) {
  let uri = Services.io.newURI(spec);
  return {
    currentURI: uri,
    contentPrincipal: Services.scriptSecurityManager.createContentPrincipal(
      uri,
      {}
    ),
    dispatchEvent: () => {},
    ownerGlobal: {
      CustomEvent: class CustomEvent {},
    },
  };
}

/**
 * Tests that temporary permissions with different block states are stored
 * (set, overwrite, delete) correctly.
 */
add_task(async function testAllowBlock() {
  // Set two temporary permissions on the same browser.
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );

  SitePermissions.setForPrincipal(
    null,
    PERM_B,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );

  // Test that the permissions have been set correctly.
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, BROWSER_A),
    {
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "SitePermissions returns expected permission state for perm A."
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_B, BROWSER_A),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "SitePermissions returns expected permission state for perm B."
  );

  Assert.deepEqual(
    TemporaryPermissions.get(BROWSER_A, PERM_A),
    {
      id: PERM_A,
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "TemporaryPermissions returns expected permission state for perm A."
  );

  Assert.deepEqual(
    TemporaryPermissions.get(BROWSER_A, PERM_B),
    {
      id: PERM_B,
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "TemporaryPermissions returns expected permission state for perm B."
  );

  // Test internal data structure of TemporaryPermissions.
  let entry = TemporaryPermissions._stateByBrowser.get(BROWSER_A);
  ok(entry, "Should have an entry for browser A");
  ok(
    !TemporaryPermissions._stateByBrowser.has(BROWSER_B),
    "Should have no entry for browser B"
  );

  let { browser, uriToPerm } = entry;
  Assert.equal(
    browser?.get(),
    BROWSER_A,
    "Entry should have a weak reference to the browser."
  );

  ok(uriToPerm, "Entry should have uriToPerm object.");
  Assert.equal(Object.keys(uriToPerm).length, 2, "uriToPerm has 2 entries.");

  let permissionsA = uriToPerm[BROWSER_A.currentURI.prePath];
  let permissionsB =
    uriToPerm[Services.eTLD.getBaseDomain(BROWSER_A.currentURI)];

  ok(permissionsA, "Allow should be keyed under prePath");
  ok(permissionsB, "Block should be keyed under baseDomain");

  let permissionA = permissionsA[PERM_A];
  let permissionB = permissionsB[PERM_B];

  Assert.equal(
    permissionA.state,
    SitePermissions.ALLOW,
    "Should have correct state"
  );
  let expireTimeoutA = permissionA.expireTimeout;
  Assert.ok(
    Number.isInteger(expireTimeoutA),
    "Should have valid expire timeout"
  );

  Assert.equal(
    permissionB.state,
    SitePermissions.BLOCK,
    "Should have correct state"
  );
  let expireTimeoutB = permissionB.expireTimeout;
  Assert.ok(
    Number.isInteger(expireTimeoutB),
    "Should have valid expire timeout"
  );

  // Overwrite permission A.
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_B
  );

  Assert.ok(
    permissionsA[PERM_A].expireTimeout != expireTimeoutA,
    "Overwritten permission A should have new timer"
  );

  // Overwrite permission B - this time with a non-block state which means it
  // should be keyed by URI prePath now.
  SitePermissions.setForPrincipal(
    null,
    PERM_B,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );

  let baseDomainEntry =
    uriToPerm[Services.eTLD.getBaseDomain(BROWSER_A.currentURI)];
  Assert.ok(
    !baseDomainEntry || !baseDomainEntry[PERM_B],
    "Should not longer have baseDomain permission entry"
  );

  permissionsB = uriToPerm[BROWSER_A.currentURI.prePath];
  permissionB = permissionsB[PERM_B];
  Assert.ok(
    permissionsB && permissionB,
    "Overwritten permission should be keyed under prePath"
  );
  Assert.equal(
    permissionB.state,
    SitePermissions.ALLOW,
    "Should have correct updated state"
  );
  Assert.ok(
    permissionB.expireTimeout != expireTimeoutB,
    "Overwritten permission B should have new timer"
  );

  // Remove permissions
  SitePermissions.removeFromPrincipal(null, PERM_A, BROWSER_A);
  SitePermissions.removeFromPrincipal(null, PERM_B, BROWSER_A);

  // Test that permissions have been removed correctly
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, BROWSER_A),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "SitePermissions returns UNKNOWN state for A."
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_B, BROWSER_A),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "SitePermissions returns UNKNOWN state for B."
  );

  Assert.equal(
    TemporaryPermissions.get(BROWSER_A, PERM_A),
    null,
    "TemporaryPermissions returns null for perm A."
  );

  Assert.equal(
    TemporaryPermissions.get(BROWSER_A, PERM_B),
    null,
    "TemporaryPermissions returns null for perm B."
  );
});

/**
 * Tests TemporaryPermissions#getAll.
 */
add_task(async function testGetAll() {
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );
  SitePermissions.setForPrincipal(
    null,
    PERM_B,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_B,
    EXPIRY_MS_A
  );
  SitePermissions.setForPrincipal(
    null,
    PERM_C,
    SitePermissions.PROMPT,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_B,
    EXPIRY_MS_A
  );

  Assert.deepEqual(TemporaryPermissions.getAll(BROWSER_A), [
    {
      id: PERM_A,
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
  ]);

  let permsBrowserB = TemporaryPermissions.getAll(BROWSER_B);
  Assert.equal(
    permsBrowserB.length,
    2,
    "There should be 2 permissions set for BROWSER_B"
  );

  let permB;
  let permC;

  if (permsBrowserB[0].id == PERM_B) {
    permB = permsBrowserB[0];
    permC = permsBrowserB[1];
  } else {
    permB = permsBrowserB[1];
    permC = permsBrowserB[0];
  }

  Assert.deepEqual(permB, {
    id: PERM_B,
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_TEMPORARY,
  });
  Assert.deepEqual(permC, {
    id: PERM_C,
    state: SitePermissions.PROMPT,
    scope: SitePermissions.SCOPE_TEMPORARY,
  });
});

/**
 * Tests SitePermissions#clearTemporaryBlockPermissions and
 * TemporaryPermissions#clear.
 */
add_task(async function testClear() {
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );
  SitePermissions.setForPrincipal(
    null,
    PERM_B,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_A
  );
  SitePermissions.setForPrincipal(
    null,
    PERM_C,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_B,
    EXPIRY_MS_A
  );

  let stateByBrowser = SitePermissions._temporaryPermissions._stateByBrowser;

  Assert.ok(stateByBrowser.has(BROWSER_A), "Browser map should have BROWSER_A");
  Assert.ok(stateByBrowser.has(BROWSER_B), "Browser map should have BROWSER_B");

  SitePermissions.clearTemporaryBlockPermissions(BROWSER_A);

  // We only clear block permissions, so we should still see PERM_A.
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, BROWSER_A),
    {
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "SitePermissions returns ALLOW state for PERM_A."
  );
  // We don't clear BROWSER_B so it should still be there.
  Assert.ok(stateByBrowser.has(BROWSER_B), "Should still have BROWSER_B.");

  // Now clear allow permissions for A explicitly.
  SitePermissions._temporaryPermissions.clear(BROWSER_A, SitePermissions.ALLOW);

  Assert.ok(!stateByBrowser.has(BROWSER_A), "Should no longer have BROWSER_A.");
  let browser = stateByBrowser.get(BROWSER_B);
  Assert.ok(browser, "Should still have BROWSER_B");

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, BROWSER_A),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "SitePermissions returns UNKNOWN state for PERM_A."
  );
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_B, BROWSER_A),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "SitePermissions returns UNKNOWN state for PERM_B."
  );
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_C, BROWSER_B),
    {
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "SitePermissions returns BLOCK state for PERM_C."
  );

  SitePermissions._temporaryPermissions.clear(BROWSER_B);

  Assert.ok(!stateByBrowser.has(BROWSER_B), "Should no longer have BROWSER_B.");
  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_C, BROWSER_B),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "SitePermissions returns UNKNOWN state for PERM_C."
  );
});

/**
 * Tests that the temporary permissions setter calls the callback on permission
 * expire with the associated browser.
 */
add_task(async function testCallbackOnExpiry() {
  let promiseExpireA = new Promise(resolve => {
    TemporaryPermissions.set(
      BROWSER_A,
      PERM_A,
      SitePermissions.BLOCK,
      100,
      BROWSER_A.currentURI,
      resolve
    );
  });
  let promiseExpireB = new Promise(resolve => {
    TemporaryPermissions.set(
      BROWSER_B,
      PERM_A,
      SitePermissions.BLOCK,
      100,
      BROWSER_B.currentURI,
      resolve
    );
  });

  let [browserA, browserB] = await Promise.all([
    promiseExpireA,
    promiseExpireB,
  ]);
  Assert.equal(
    browserA,
    BROWSER_A,
    "Should get callback with browser on expiry for A"
  );
  Assert.equal(
    browserB,
    BROWSER_B,
    "Should get callback with browser on expiry for B"
  );
});

/**
 * Tests that the temporary permissions setter calls the callback on permission
 * expire with the associated browser if the browser associated browser has
 * changed after setting the permission.
 */
add_task(async function testCallbackOnExpiryUpdatedBrowser() {
  let promiseExpire = new Promise(resolve => {
    TemporaryPermissions.set(
      BROWSER_A,
      PERM_A,
      SitePermissions.BLOCK,
      200,
      BROWSER_A.currentURI,
      resolve
    );
  });

  TemporaryPermissions.copy(BROWSER_A, BROWSER_B);

  let browser = await promiseExpire;
  Assert.equal(
    browser,
    BROWSER_B,
    "Should get callback with updated browser on expiry."
  );
});

/**
 * Tests that the permission setter throws an exception if an invalid expiry
 * time is passed.
 */
add_task(async function testInvalidExpiryTime() {
  let expectedError = /expireTime must be a positive integer/;
  Assert.throws(() => {
    SitePermissions.setForPrincipal(
      null,
      PERM_A,
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_TEMPORARY,
      BROWSER_A,
      null
    );
  }, expectedError);
  Assert.throws(() => {
    SitePermissions.setForPrincipal(
      null,
      PERM_A,
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_TEMPORARY,
      BROWSER_A,
      0
    );
  }, expectedError);
  Assert.throws(() => {
    SitePermissions.setForPrincipal(
      null,
      PERM_A,
      SitePermissions.ALLOW,
      SitePermissions.SCOPE_TEMPORARY,
      BROWSER_A,
      -100
    );
  }, expectedError);
});

/**
 * Tests that we block by base domain but allow by prepath.
 */
add_task(async function testTemporaryPermissionScope() {
  let states = {
    strict: {
      same: [
        "https://example.com",
        "https://example.com/sub/path",
        "https://example.com:443",
      ],
      different: [
        "https://example.com",
        "https://name:password@example.com",
        "https://test1.example.com",
        "http://example.com",
        "http://example.org",
      ],
    },
    nonStrict: {
      same: [
        "https://example.com",
        "https://example.com/sub/path",
        "https://example.com:443",
        "https://test1.example.com",
        "http://test2.test1.example.com",
        "https://name:password@example.com",
        "http://example.com",
      ],
      different: [
        "https://example.com",
        "https://example.org",
        "http://example.net",
      ],
    },
  };

  for (let state of [SitePermissions.BLOCK, SitePermissions.ALLOW]) {
    let matchStrict = state != SitePermissions.BLOCK;

    let lists = matchStrict ? states.strict : states.nonStrict;

    Object.entries(lists).forEach(([type, list]) => {
      let expectSet = type == "same";

      for (let uri of list) {
        let browser = createDummyBrowser(uri);
        SitePermissions.setForPrincipal(
          null,
          PERM_A,
          state,
          SitePermissions.SCOPE_TEMPORARY,
          browser,
          EXPIRY_MS_A
        );

        for (let otherUri of list) {
          if (uri == otherUri) {
            continue;
          }
          browser.currentURI = Services.io.newURI(otherUri);

          Assert.deepEqual(
            SitePermissions.getForPrincipal(null, PERM_A, browser),
            {
              state: expectSet ? state : SitePermissions.UNKNOWN,
              scope: expectSet
                ? SitePermissions.SCOPE_TEMPORARY
                : SitePermissions.SCOPE_PERSISTENT,
            },
            `${
              state == SitePermissions.BLOCK ? "Block" : "Allow"
            } Permission originally set for ${uri} should ${
              expectSet ? "not" : "also"
            } be set for ${otherUri}.`
          );
        }

        SitePermissions._temporaryPermissions.clear(browser);
      }
    });
  }
});

/**
 * Tests that we can override the URI to use for keying temporary permissions.
 */
add_task(async function testOverrideBrowserURI() {
  let testBrowser = createDummyBrowser("https://old.example.com/foo");
  let overrideURI = Services.io.newURI("https://test.example.org/test/path");
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    testBrowser,
    EXPIRY_MS_A,
    overrideURI
  );

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, testBrowser),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "Permission should not be set for old URI."
  );

  // "Navigate" to new URI
  testBrowser.currentURI = overrideURI;

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, testBrowser),
    {
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_TEMPORARY,
    },
    "Permission should be set for new URI."
  );

  SitePermissions._temporaryPermissions.clear(testBrowser);
});

/**
 * Tests that TemporaryPermissions does not throw for incompatible URI or
 * browser.currentURI.
 */
add_task(async function testPermissionUnsupportedScheme() {
  let aboutURI = Services.io.newURI("about:blank");

  // Incompatible override URI should not throw or store any permissions.
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    BROWSER_A,
    EXPIRY_MS_B,
    aboutURI
  );
  Assert.ok(
    SitePermissions._temporaryPermissions._stateByBrowser.has(BROWSER_A),
    "Should not have stored permission for unsupported URI scheme."
  );

  let browser = createDummyBrowser("https://example.com/");
  // Set a permission so we get an entry in the browser map.
  SitePermissions.setForPrincipal(
    null,
    PERM_B,
    SitePermissions.BLOCK,
    SitePermissions.SCOPE_TEMPORARY,
    browser
  );

  // Change browser URI to about:blank.
  browser.currentURI = aboutURI;

  // Setting permission for browser with unsupported URI should not throw.
  SitePermissions.setForPrincipal(
    null,
    PERM_A,
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_TEMPORARY,
    browser
  );
  Assert.ok(true, "Set should not throw for unsupported URI");

  SitePermissions.removeFromPrincipal(null, PERM_A, browser);
  Assert.ok(true, "Remove should not throw for unsupported URI");

  Assert.deepEqual(
    SitePermissions.getForPrincipal(null, PERM_A, browser),
    {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    "Should return no permission set for unsupported URI."
  );
  Assert.ok(true, "Get should not throw for unsupported URI");

  // getAll should not throw, but return empty permissions array.
  let permissions = SitePermissions.getAllForBrowser(browser);
  Assert.ok(
    Array.isArray(permissions) && !permissions.length,
    "Should return empty array for browser on about:blank"
  );

  SitePermissions._temporaryPermissions.clear(browser);
});
