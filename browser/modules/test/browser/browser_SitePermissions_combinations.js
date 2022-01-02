/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

// This function applies combinations of different permissions and
// checks how they override each other.
async function checkPermissionCombinations(combinations) {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );

  await BrowserTestUtils.withNewTab(principal.spec, function(browser) {
    let id = "geo";
    for (let { reverse, states, result } of combinations) {
      let loop = () => {
        for (let [state, scope] of states) {
          SitePermissions.setForPrincipal(principal, id, state, scope, browser);
        }
        Assert.deepEqual(
          SitePermissions.getForPrincipal(principal, id, browser),
          result
        );
        SitePermissions.removeFromPrincipal(principal, id, browser);
      };

      loop();

      if (reverse) {
        states.reverse();
        loop();
      }
    }
  });
}

// Test that passing null as scope becomes SCOPE_PERSISTENT.
add_task(async function testDefaultScope() {
  await checkPermissionCombinations([
    {
      states: [[SitePermissions.ALLOW, null]],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
  ]);
});

// Test that "wide" scopes like PERSISTENT always override "narrower" ones like TAB.
add_task(async function testScopeOverrides() {
  await checkPermissionCombinations([
    {
      // The behavior of SCOPE_SESSION is not in line with the general behavior
      // because of the legacy nsIPermissionManager implementation.
      states: [
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.BLOCK, SitePermissions.SCOPE_SESSION],
      ],
      result: {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_SESSION,
      },
    },
    {
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_SESSION],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
    {
      reverse: true,
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_SESSION],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_SESSION,
      },
    },
    {
      reverse: true,
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
  ]);
});

// Test that clearing a temporary permission also removes a
// persistent permission that was set for the same URL.
add_task(async function testClearTempPermission() {
  await checkPermissionCombinations([
    {
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.UNKNOWN, SitePermissions.SCOPE_TEMPORARY],
      ],
      result: {
        state: SitePermissions.UNKNOWN,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
  ]);
});

// Test that states override each other when applied with the same scope.
add_task(async function testStateOverride() {
  await checkPermissionCombinations([
    {
      states: [
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.BLOCK, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
    {
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    },
  ]);
});
