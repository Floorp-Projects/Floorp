/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

async function testManifest(manifest, expectedError) {
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);

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
  // The above test validates these work with the correct version,
  // here we verify they fail with the incorrect version.
  testManifest(
    {
      manifest_version: 2,
      action: {
        default_panel: "foo.html",
      },
    },
    /Property "action" is unsupported in Manifest Version 2/
  );
  testManifest(
    {
      manifest_version: 3,
      browser_action: {
        default_panel: "foo.html",
      },
    },
    /Property "browser_action" is unsupported in Manifest Version 3/
  );
});
