"use strict";

/**
 * Helper function for testing a theme with invalid properties.
 * @param {object} invalidProps The invalid properties to load the theme with.
 */
async function testThemeWithInvalidProperties(invalidProps) {
  let manifest = {
    theme: {},
  };

  invalidProps.forEach(prop => {
    // Some properties require additional information:
    switch (prop) {
      case "background":
        manifest[prop] = { scripts: ["background.js"] };
        break;
      case "permissions":
        manifest[prop] = ["tabs"];
        break;
      case "omnibox":
        manifest[prop] = { keyword: "test" };
        break;
      default:
        manifest[prop] = {};
    }
  });

  let extension = ExtensionTestUtils.loadExtension({ manifest });
  await Assert.rejects(
    extension.startup(),
    /startup failed/,
    "Theme should fail to load if it contains invalid properties"
  );
}

add_task(
  async function test_that_theme_with_invalid_properties_fails_to_load() {
    let invalidProps = [
      "page_action",
      "browser_action",
      "background",
      "permissions",
      "omnibox",
      "commands",
    ];
    for (let prop in invalidProps) {
      await testThemeWithInvalidProperties([prop]);
    }
    await testThemeWithInvalidProperties(invalidProps);
  }
);
