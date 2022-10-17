/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

async function testManifest(manifest, expectedError) {
  ExtensionTestUtils.failOnSchemaWarnings(false);
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);
  ExtensionTestUtils.failOnSchemaWarnings(true);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(manifest)}, got ${
        normalized.error
      }`
    );
  } else {
    ok(
      !normalized.error,
      `Should not have an error ${JSON.stringify(manifest)}, ${
        normalized.error
      }`
    );
  }
  return normalized.errors;
}

const all_actions = [
  "action",
  "browser_action",
  "page_action",
  "sidebar_action",
];

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    for (let action of all_actions) {
      let manifest_version = action == "action" ? 3 : 2;
      let manifest = { manifest_version };
      manifest[action] = { default_icon: path };
      let error = new RegExp(`Error processing ${action}.default_icon`);
      await testManifest(manifest, error);

      manifest[action] = { default_icon: { "16": path } };
      await testManifest(manifest, error);
    }
  }

  let paths = [
    "icon.png",
    "/icon.png",
    "./icon.png",
    "path to an icon.png",
    " icon.png",
  ];
  for (let path of paths) {
    for (let action of all_actions) {
      let manifest_version = action == "action" ? 3 : 2;
      let manifest = { manifest_version };
      manifest[action] = { default_icon: path };
      if (action == "sidebar_action") {
        // Sidebar requires panel.
        manifest[action].default_panel = "foo.html";
      }
      await testManifest(manifest);

      manifest[action] = { default_icon: { "16": path } };
      if (action == "sidebar_action") {
        manifest[action].default_panel = "foo.html";
      }
      await testManifest(manifest);
    }
  }
});

add_task(async function test_action_version() {
  let warnings = await testManifest({
    manifest_version: 3,
    browser_action: {
      default_panel: "foo.html",
    },
  });
  Assert.deepEqual(
    warnings,
    [`Property "browser_action" is unsupported in Manifest Version 3`],
    `Manifest v3 with "browser_action" key logs an error.`
  );

  warnings = await testManifest({
    manifest_version: 2,
    action: {
      default_icon: "",
      default_panel: "foo.html",
    },
  });

  Assert.deepEqual(
    warnings,
    [`Property "action" is unsupported in Manifest Version 2`],
    `Manifest v2 with "action" key first warning is clear.`
  );
});

add_task(async function test_mv2_scripting_permission_always_enabled() {
  let warnings = await testManifest({
    manifest_version: 2,
    permissions: ["scripting"],
  });

  Assert.deepEqual(warnings, [], "Got no warnings");
});

add_task(async function test_mv3_scripting_permission_always_enabled() {
  let warnings = await testManifest({
    manifest_version: 3,
    permissions: ["scripting"],
  });

  Assert.deepEqual(warnings, [], "Got no warnings");
});

add_task(async function test_simpler_version_format() {
  const TEST_CASES = [
    // Valid cases
    { version: "0", expectWarning: false },
    { version: "0.0", expectWarning: false },
    { version: "0.0.0", expectWarning: false },
    { version: "0.0.0.0", expectWarning: false },
    { version: "0.0.0.1", expectWarning: false },
    { version: "0.0.0.999999999", expectWarning: false },
    { version: "0.0.1.0", expectWarning: false },
    { version: "0.0.999999999", expectWarning: false },
    { version: "0.1.0.0", expectWarning: false },
    { version: "0.999999999", expectWarning: false },
    { version: "1", expectWarning: false },
    { version: "1.0", expectWarning: false },
    { version: "1.0.0", expectWarning: false },
    { version: "1.0.0.0", expectWarning: false },
    { version: "1.2.3.4", expectWarning: false },
    { version: "999999999", expectWarning: false },
    {
      version: "999999999.999999999.999999999.999999999",
      expectWarning: false,
    },
    // Invalid cases
    { version: ".", expectWarning: true },
    { version: ".999999999", expectWarning: true },
    { version: "0.0.0.0.0", expectWarning: true },
    { version: "0.0.0.00001", expectWarning: true },
    { version: "0.0.0.0010", expectWarning: true },
    { version: "0.0.00001", expectWarning: true },
    { version: "0.0.001", expectWarning: true },
    { version: "0.0.01.0", expectWarning: true },
    { version: "0.01.0", expectWarning: true },
    { version: "00001", expectWarning: true },
    { version: "0001", expectWarning: true },
    { version: "001", expectWarning: true },
    { version: "01", expectWarning: true },
    { version: "01.0", expectWarning: true },
    { version: "099999", expectWarning: true },
    { version: "0999999999", expectWarning: true },
    { version: "1.00000", expectWarning: true },
    { version: "1.1.-1", expectWarning: true },
    { version: "1.1000000000", expectWarning: true },
    { version: "1.1pre1aa", expectWarning: true },
    { version: "1.2.1000000000", expectWarning: true },
    { version: "1.2.3.4-a", expectWarning: true },
    { version: "1.2.3.4.5", expectWarning: true },
    { version: "1000000000", expectWarning: true },
    { version: "1000000000.0.0.0", expectWarning: true },
    { version: "999999999.", expectWarning: true },
  ];

  for (const { version, expectWarning } of TEST_CASES) {
    const normalized = await ExtensionTestUtils.normalizeManifest({ version });

    if (expectWarning) {
      Assert.deepEqual(
        normalized.errors,
        [
          `Warning processing version: version must be a version string ` +
            `consisting of at most 4 integers of at most 9 digits without ` +
            `leading zeros, and separated with dots`,
        ],
        `expected warning for version: ${version}`
      );
    } else {
      Assert.deepEqual(
        normalized.errors,
        [],
        `expected no warning for version: ${version}`
      );
    }
  }
});
