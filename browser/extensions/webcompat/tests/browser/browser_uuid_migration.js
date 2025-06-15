"use strict";

// Sanity check: The setup in the toml file is effective at fixing the uuid
// for at least one extension.
add_task(async function test_sanity_check_uuid_fixed_by_pref() {
  is(
    WebExtensionPolicy.getByID("pictureinpicture@mozilla.org")
      .mozExtensionHostname,
    "f00df00d-2222-f00d-8888-012345678900",
    "Non-webcompat uuid is fixed by pref in browser_uuid_migration.toml"
  );
});

add_task(async function test_webcompat_migrates_existing_uuid_pref() {
  const expectedWebCompatUUID = "9a310967-e580-48bf-b3e8-4eafebbc122d";
  Assert.notEqual(
    expectedWebCompatUUID,
    "f00df00d-1111-f00d-8888-012345678900",
    "Sanity check: Expected UUID differs from browser_uuid_migration.toml"
  );

  is(
    WebExtensionPolicy.getByID("webcompat@mozilla.org").mozExtensionHostname,
    expectedWebCompatUUID,
    "webcompat add-on has fixed UUID"
  );

  let uuids = Services.prefs.getStringPref("extensions.webextensions.uuids");
  ok(
    uuids.includes(`"webcompat@mozilla.org":"${expectedWebCompatUUID}"`),
    `Pref value (${uuids}) should contain: ${expectedWebCompatUUID}`
  );
});
