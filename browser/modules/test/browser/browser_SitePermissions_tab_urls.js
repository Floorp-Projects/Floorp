/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

function newPrincipal(origin) {
  return Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    origin
  );
}

// This tests the key used to store the URI -> permission map on a tab.
add_task(async function testTemporaryPermissionTabURLs() {
  // Prevent showing a dialog for https://name:password@example.com
  SpecialPowers.pushPrefEnv({
    set: [["network.http.phishy-userpass-length", 2048]],
  });

  // This usually takes about 60 seconds on 32bit Linux debug,
  // due to the combinatory nature of the test that is hard to fix.
  requestLongerTimeout(2);

  let same = [
    newPrincipal("https://example.com"),
    newPrincipal("https://example.com:443"),
    newPrincipal("https://test1.example.com"),
    newPrincipal("https://name:password@example.com"),
    newPrincipal("http://example.com"),
  ];
  let different = [
    newPrincipal("https://example.com"),
    newPrincipal("http://example.org"),
    newPrincipal("http://example.net"),
  ];

  let id = "microphone";

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    for (let principal of same) {
      let loaded = BrowserTestUtils.browserLoaded(
        browser,
        false,
        principal.spec
      );
      BrowserTestUtils.loadURI(browser, principal.spec);
      await loaded;

      SitePermissions.setForPrincipal(
        principal,
        id,
        SitePermissions.BLOCK,
        SitePermissions.SCOPE_TEMPORARY,
        browser
      );

      for (let principal2 of same) {
        let loaded2 = BrowserTestUtils.browserLoaded(
          browser,
          false,
          principal2.URI.spec
        );
        BrowserTestUtils.loadURI(browser, principal2.URI.spec);
        await loaded2;

        Assert.deepEqual(
          SitePermissions.getForPrincipal(principal2, id, browser),
          {
            state: SitePermissions.BLOCK,
            scope: SitePermissions.SCOPE_TEMPORARY,
          },
          `${principal.spec} should share tab permissions with ${principal2.spec}`
        );
      }

      SitePermissions.clearTemporaryBlockPermissions(browser);
    }

    for (let principal of different) {
      let loaded = BrowserTestUtils.browserLoaded(
        browser,
        false,
        principal.spec
      );
      BrowserTestUtils.loadURI(browser, principal.spec);
      await loaded;

      SitePermissions.setForPrincipal(
        principal,
        id,
        SitePermissions.BLOCK,
        SitePermissions.SCOPE_TEMPORARY,
        browser
      );

      Assert.deepEqual(
        SitePermissions.getForPrincipal(principal, id, browser),
        {
          state: SitePermissions.BLOCK,
          scope: SitePermissions.SCOPE_TEMPORARY,
        }
      );

      for (let principal2 of different) {
        loaded = BrowserTestUtils.browserLoaded(
          browser,
          false,
          principal2.URI.spec
        );
        BrowserTestUtils.loadURI(browser, principal2.URI.spec);
        await loaded;

        if (principal2 != principal) {
          Assert.deepEqual(
            SitePermissions.getForPrincipal(principal2, id, browser),
            {
              state: SitePermissions.UNKNOWN,
              scope: SitePermissions.SCOPE_PERSISTENT,
            },
            `${principal.spec} should not share tab permissions with ${principal2.spec}`
          );
        }
      }

      SitePermissions.clearTemporaryBlockPermissions(browser);
    }
  });
});
