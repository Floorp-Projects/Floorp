/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function testIconPaths(icon, manifest, expectedError) {
  let normalized = await ExtensionTestUtils.normalizeManifest(manifest);

  if (expectedError) {
    ok(
      expectedError.test(normalized.error),
      `Should have an error for ${JSON.stringify(manifest)}`
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

add_task(async function test_manifest() {
  let badpaths = ["", " ", "\t", "http://foo.com/icon.png"];
  for (let path of badpaths) {
    for (let action of ["browser_action", "page_action", "sidebar_action"]) {
      let manifest = {};
      manifest[action] = { default_icon: path };
      let error = new RegExp(`Error processing ${action}.default_icon`);
      await testIconPaths(path, manifest, error);

      manifest[action] = { default_icon: { "16": path } };
      await testIconPaths(path, manifest, error);
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
    for (let action of ["browser_action", "page_action", "sidebar_action"]) {
      let manifest = {};
      manifest[action] = { default_icon: path };
      if (action == "sidebar_action") {
        // Sidebar requires panel.
        manifest[action].default_panel = "foo.html";
      }
      await testIconPaths(path, manifest);

      manifest[action] = { default_icon: { "16": path } };
      if (action == "sidebar_action") {
        manifest[action].default_panel = "foo.html";
      }
      await testIconPaths(path, manifest);
    }
  }
});
