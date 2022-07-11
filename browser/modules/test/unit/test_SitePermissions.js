/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { SitePermissions } = ChromeUtils.import(
  "resource:///modules/SitePermissions.jsm"
);

const RESIST_FINGERPRINTING_ENABLED = Services.prefs.getBoolPref(
  "privacy.resistFingerprinting"
);
const MIDI_ENABLED = Services.prefs.getBoolPref("dom.webmidi.enabled");

const EXT_PROTOCOL_ENABLED = Services.prefs.getBoolPref(
  "security.external_protocol_requires_permission"
);

add_task(async function testPermissionsListing() {
  let expectedPermissions = [
    "autoplay-media",
    "camera",
    "cookie",
    "desktop-notification",
    "focus-tab-by-prompt",
    "geo",
    "install",
    "microphone",
    "popup",
    "screen",
    "shortcuts",
    "persistent-storage",
    "storage-access",
    "xr",
    "3rdPartyStorage",
  ];
  if (RESIST_FINGERPRINTING_ENABLED) {
    // Canvas permission should be hidden unless privacy.resistFingerprinting
    // is true.
    expectedPermissions.push("canvas");
  }
  if (MIDI_ENABLED) {
    // Should remove this checking and add it as default after it is fully pref'd-on.
    expectedPermissions.push("midi");
    expectedPermissions.push("midi-sysex");
  }
  if (EXT_PROTOCOL_ENABLED) {
    expectedPermissions.push("open-protocol-handler");
  }
  Assert.deepEqual(
    SitePermissions.listPermissions().sort(),
    expectedPermissions.sort(),
    "Correct list of all permissions"
  );
});

add_task(async function testGetAllByPrincipal() {
  // check that it returns an empty array on an invalid principal
  // like a principal with an about URI, which doesn't support site permissions
  let wrongPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "about:config"
  );
  Assert.deepEqual(SitePermissions.getAllByPrincipal(wrongPrincipal), []);

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), []);

  SitePermissions.setForPrincipal(principal, "camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), [
    {
      id: "camera",
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  ]);

  SitePermissions.setForPrincipal(
    principal,
    "microphone",
    SitePermissions.ALLOW,
    SitePermissions.SCOPE_SESSION
  );
  SitePermissions.setForPrincipal(
    principal,
    "desktop-notification",
    SitePermissions.BLOCK
  );

  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), [
    {
      id: "camera",
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    {
      id: "microphone",
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_SESSION,
    },
    {
      id: "desktop-notification",
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  ]);

  SitePermissions.removeFromPrincipal(principal, "microphone");
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), [
    {
      id: "camera",
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
    {
      id: "desktop-notification",
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  ]);

  SitePermissions.removeFromPrincipal(principal, "camera");
  SitePermissions.removeFromPrincipal(principal, "desktop-notification");
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), []);

  Assert.equal(Services.prefs.getIntPref("permissions.default.shortcuts"), 0);
  SitePermissions.setForPrincipal(
    principal,
    "shortcuts",
    SitePermissions.BLOCK
  );

  // Customized preference should have been enabled, but the default should not.
  Assert.equal(Services.prefs.getIntPref("permissions.default.shortcuts"), 0);
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), [
    {
      id: "shortcuts",
      state: SitePermissions.BLOCK,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  ]);

  SitePermissions.removeFromPrincipal(principal, "shortcuts");
  Services.prefs.clearUserPref("permissions.default.shortcuts");
});

add_task(async function testGetAvailableStates() {
  Assert.deepEqual(SitePermissions.getAvailableStates("camera"), [
    SitePermissions.UNKNOWN,
    SitePermissions.ALLOW,
    SitePermissions.BLOCK,
  ]);

  // Test available states with a default permission set.
  Services.prefs.setIntPref(
    "permissions.default.camera",
    SitePermissions.ALLOW
  );
  Assert.deepEqual(SitePermissions.getAvailableStates("camera"), [
    SitePermissions.PROMPT,
    SitePermissions.ALLOW,
    SitePermissions.BLOCK,
  ]);
  Services.prefs.clearUserPref("permissions.default.camera");

  Assert.deepEqual(SitePermissions.getAvailableStates("cookie"), [
    SitePermissions.ALLOW,
    SitePermissions.ALLOW_COOKIES_FOR_SESSION,
    SitePermissions.BLOCK,
  ]);

  Assert.deepEqual(SitePermissions.getAvailableStates("popup"), [
    SitePermissions.ALLOW,
    SitePermissions.BLOCK,
  ]);
});

add_task(async function testExactHostMatch() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );
  let subPrincipal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://test1.example.com"
  );

  let exactHostMatched = [
    "autoplay-media",
    "desktop-notification",
    "focus-tab-by-prompt",
    "camera",
    "microphone",
    "screen",
    "geo",
    "xr",
    "persistent-storage",
  ];
  if (RESIST_FINGERPRINTING_ENABLED) {
    // Canvas permission should be hidden unless privacy.resistFingerprinting
    // is true.
    exactHostMatched.push("canvas");
  }
  if (MIDI_ENABLED) {
    // WebMIDI is only pref'd on in nightly.
    // Should remove this checking and add it as default after it is fully pref-on.
    exactHostMatched.push("midi");
    exactHostMatched.push("midi-sysex");
  }
  if (EXT_PROTOCOL_ENABLED) {
    exactHostMatched.push("open-protocol-handler");
  }
  let nonExactHostMatched = [
    "cookie",
    "popup",
    "install",
    "shortcuts",
    "storage-access",
    "3rdPartyStorage",
  ];

  let permissions = SitePermissions.listPermissions();
  for (let permission of permissions) {
    SitePermissions.setForPrincipal(
      principal,
      permission,
      SitePermissions.ALLOW
    );

    if (exactHostMatched.includes(permission)) {
      // Check that the sub-origin does not inherit the permission from its parent.
      Assert.equal(
        SitePermissions.getForPrincipal(subPrincipal, permission).state,
        SitePermissions.getDefault(permission),
        `${permission} should exact-host match`
      );
    } else if (nonExactHostMatched.includes(permission)) {
      // Check that the sub-origin does inherit the permission from its parent.
      Assert.equal(
        SitePermissions.getForPrincipal(subPrincipal, permission).state,
        SitePermissions.ALLOW,
        `${permission} should not exact-host match`
      );
    } else {
      Assert.ok(
        false,
        `Found an unknown permission ${permission} in exact host match test.` +
          "Please add new permissions from SitePermissions.jsm to this test."
      );
    }

    // Check that the permission can be made specific to the sub-origin.
    SitePermissions.setForPrincipal(
      subPrincipal,
      permission,
      SitePermissions.PROMPT
    );
    Assert.equal(
      SitePermissions.getForPrincipal(subPrincipal, permission).state,
      SitePermissions.PROMPT
    );
    Assert.equal(
      SitePermissions.getForPrincipal(principal, permission).state,
      SitePermissions.ALLOW
    );

    SitePermissions.removeFromPrincipal(subPrincipal, permission);
    SitePermissions.removeFromPrincipal(principal, permission);
  }
});

add_task(async function testDefaultPrefs() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );

  // Check that without a pref the default return value is UNKNOWN.
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the default return value changed after setting the pref.
  Services.prefs.setIntPref(
    "permissions.default.camera",
    SitePermissions.BLOCK
  );
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that other permissions still return UNKNOWN.
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "microphone"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the default return value changed after changing the pref.
  Services.prefs.setIntPref(
    "permissions.default.camera",
    SitePermissions.ALLOW
  );
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Check that the preference is ignored if there is a value.
  SitePermissions.setForPrincipal(principal, "camera", SitePermissions.BLOCK);
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.BLOCK,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // The preference should be honored again, after resetting the permissions.
  SitePermissions.removeFromPrincipal(principal, "camera");
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.ALLOW,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });

  // Should be UNKNOWN after clearing the pref.
  Services.prefs.clearUserPref("permissions.default.camera");
  Assert.deepEqual(SitePermissions.getForPrincipal(principal, "camera"), {
    state: SitePermissions.UNKNOWN,
    scope: SitePermissions.SCOPE_PERSISTENT,
  });
});

add_task(async function testCanvasPermission() {
  let resistFingerprinting = Services.prefs.getBoolPref(
    "privacy.resistFingerprinting",
    false
  );
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "https://example.com"
  );

  SitePermissions.setForPrincipal(principal, "canvas", SitePermissions.ALLOW);

  // Canvas permission is hidden when privacy.resistFingerprinting is false.
  Services.prefs.setBoolPref("privacy.resistFingerprinting", false);
  Assert.equal(SitePermissions.listPermissions().indexOf("canvas"), -1);
  Assert.equal(
    SitePermissions.getAllByPrincipal(principal).filter(
      permission => permission.id === "canvas"
    ).length,
    0
  );

  // Canvas permission is visible when privacy.resistFingerprinting is true.
  Services.prefs.setBoolPref("privacy.resistFingerprinting", true);
  Assert.notEqual(SitePermissions.listPermissions().indexOf("canvas"), -1);
  Assert.notEqual(
    SitePermissions.getAllByPrincipal(principal).filter(
      permission => permission.id === "canvas"
    ).length,
    0
  );

  SitePermissions.removeFromPrincipal(principal, "canvas");
  Services.prefs.setBoolPref(
    "privacy.resistFingerprinting",
    resistFingerprinting
  );
});

add_task(async function testFilePermissions() {
  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    "file:///example.js"
  );
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), []);

  SitePermissions.setForPrincipal(principal, "camera", SitePermissions.ALLOW);
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), [
    {
      id: "camera",
      state: SitePermissions.ALLOW,
      scope: SitePermissions.SCOPE_PERSISTENT,
    },
  ]);
  SitePermissions.removeFromPrincipal(principal, "camera");
  Assert.deepEqual(SitePermissions.getAllByPrincipal(principal), []);
});
