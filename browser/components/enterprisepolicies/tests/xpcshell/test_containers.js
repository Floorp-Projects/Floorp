/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  ContextualIdentityService:
    "resource://gre/modules/ContextualIdentityService.sys.mjs",
});

add_task(async function setup() {
  Services.prefs.setBoolPref("privacy.userContext.enabled", true);
  do_get_profile();
});

add_task(async function test_containers_default() {
  await setupPolicyEngineWithJson({
    policies: {
      Containers: {
        Default: [
          {
            name: "Test Container",
            icon: "cart",
            color: "orange",
          },
        ],
      },
    },
  });
  let identities = ContextualIdentityService.getPublicIdentities();
  equal(identities.length, 1);
  ContextualIdentityService.getPublicIdentities().forEach(identity => {
    equal(identity.name, "Test Container");
    equal(identity.icon, "cart");
    equal(identity.color, "orange");
  });
});
