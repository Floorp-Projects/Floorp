/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// TODO (Bug 1800937): Remove this whole test along with the migration code
// after the next watershed release.

const { ASRouterNewTabHook } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterNewTabHook.sys.mjs"
);
const { ASRouterDefaultConfig } = ChromeUtils.importESModule(
  "resource:///modules/asrouter/ASRouterDefaultConfig.sys.mjs"
);

add_setup(() => ASRouterNewTabHook.destroy());

// Test that the old pref format is migrated correctly to the new format.
// provider.bucket -> provider.collection
add_task(async function test_newtab_asrouter() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.asrouter.providers.cfr",
        JSON.stringify({
          id: "cfr",
          enabled: true,
          type: "local",
          bucket: "cfr", // The pre-migration property name is bucket.
          updateCyleInMs: 3600000,
        }),
      ],
    ],
  });

  await ASRouterNewTabHook.createInstance(ASRouterDefaultConfig());
  const hook = await ASRouterNewTabHook.getInstance();
  const router = hook._router;
  if (!router.initialized) {
    await router.waitForInitialized;
  }

  // Test that the pref's bucket is migrated to collection.
  let cfrProvider = router.state.providers.find(p => p.id === "cfr");
  Assert.equal(cfrProvider.collection, "cfr", "The collection name is correct");
  Assert.ok(!cfrProvider.bucket, "The bucket name is removed");
});
