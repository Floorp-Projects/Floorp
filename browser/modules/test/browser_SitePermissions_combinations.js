/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource:///modules/SitePermissions.jsm", this);

// This function applies combinations of different permissions and
// checks how they override each other.
function* checkPermissionCombinations(combinations) {
  let uri = Services.io.newURI("https://example.com");

  yield BrowserTestUtils.withNewTab(uri.spec, function(browser) {
    let id = "geo";
    for (let {reverse, states, result} of combinations) {
      let loop = () => {
        for (let [state, scope] of states) {
          SitePermissions.set(uri, id, state, scope, browser);
        }
        Assert.deepEqual(SitePermissions.get(uri, id, browser), result);
        SitePermissions.remove(uri, id, browser);
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
add_task(function* testDefaultScope() {
  yield checkPermissionCombinations([{
    states: [
      [SitePermissions.ALLOW, null],
    ],
    result: {
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  }]);
});

// Test that "wide" scopes like PERSISTENT always override "narrower" ones like TAB.
add_task(function* testScopeOverrides() {
  yield checkPermissionCombinations([
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
    }, {
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_SESSION],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },

    }, {
      reverse: true,
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_SESSION],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_SESSION,
      },
    }, {
      reverse: true,
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    }
  ]);
});

// Test that clearing a temporary permission also removes a
// persistent permission that was set for the same URL.
add_task(function* testClearTempPermission() {
  yield checkPermissionCombinations([{
    states: [
      [SitePermissions.BLOCK, SitePermissions.SCOPE_TEMPORARY],
      [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      [SitePermissions.UNKNOWN, SitePermissions.SCOPE_TEMPORARY],
    ],
    result: {
      state: SitePermissions.UNKNOWN,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  }]);
});

// Test that states override each other when applied with the same scope.
add_task(function* testStateOverride() {
  yield checkPermissionCombinations([
    {
      states: [
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.BLOCK, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.BLOCK,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    }, {
      states: [
        [SitePermissions.BLOCK, SitePermissions.SCOPE_PERSISTENT],
        [SitePermissions.ALLOW, SitePermissions.SCOPE_PERSISTENT],
      ],
      result: {
        state: SitePermissions.ALLOW,
        scope: SitePermissions.SCOPE_PERSISTENT,
      },
    }
  ]);
});

