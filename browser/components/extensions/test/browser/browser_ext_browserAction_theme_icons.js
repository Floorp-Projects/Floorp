/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// PNG image data for a simple red dot.
const BACKGROUND = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";

const LIGHT_THEME_COLORS = {
  "accentcolor": "#FFF",
  "textcolor": "#000",
};

const DARK_THEME_COLORS = {
  "accentcolor": "#000",
  "textcolor": "#FFF",
};

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.webextensions.themes.enabled", true]],
  });
});

async function testBrowserAction(extension, expectedIcon) {
  let browserActionWidget = getBrowserActionWidget(extension);
  await promiseAnimationFrame();
  let browserActionButton = browserActionWidget.forWindow(window).node;
  ok(getListStyleImage(browserActionButton).includes(expectedIcon), `Expected browser action icon to be ${expectedIcon}`);
}

async function testStaticTheme(options) {
  let {themeType, themeIcons, withDefaultIcon} = options;

  let manifest = {
    "browser_action": {
      "theme_icons": themeIcons,
    },
  };

  if (withDefaultIcon) {
    manifest.browser_action.default_icon = "default.png";
  }

  let extension = ExtensionTestUtils.loadExtension({manifest});

  await extension.startup();

  // Confirm that the browser action has the correct default icon before a theme is loaded.
  let expectedDefaultIcon = withDefaultIcon ? "default.png" : "light.png";
  await testBrowserAction(extension, expectedDefaultIcon);

  let theme = ExtensionTestUtils.loadExtension({
    manifest: {
      "theme": {
        "images": {
          "headerURL": "background.png",
        },
        "colors": themeType === "light" ? LIGHT_THEME_COLORS : DARK_THEME_COLORS,
      },
    },
    files: {
      "background.png": BACKGROUND,
    },
  });

  await theme.startup();

  // Confirm that the correct icon is used when the theme is loaded.
  if (themeType == "light") {
    // The dark icon should be used if the theme is light.
    await testBrowserAction(extension, "dark.png");
  } else {
    // The light icon should be used if the theme is dark.
    await testBrowserAction(extension, "light.png");
  }

  await theme.unload();

  // Confirm that the browser action has the correct default icon when the theme is unloaded.
  await testBrowserAction(extension, expectedDefaultIcon);

  await extension.unload();
}

add_task(async function browseraction_theme_icons_light_theme() {
  await testStaticTheme({
    themeType: "light",
    themeIcons: [{
      "light": "light.png",
      "dark": "dark.png",
      "size": 19,
    }],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeType: "light",
    themeIcons: [{
      "light": "light.png",
      "dark": "dark.png",
      "size": 16,
    }, {
      "light": "light.png",
      "dark": "dark.png",
      "size": 32,
    }],
    withDefaultIcon: false,
  });
});

add_task(async function browseraction_theme_icons_dark_theme() {
  await testStaticTheme({
    themeType: "dark",
    themeIcons: [{
      "light": "light.png",
      "dark": "dark.png",
      "size": 19,
    }],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeType: "dark",
    themeIcons: [{
      "light": "light.png",
      "dark": "dark.png",
      "size": 16,
    }, {
      "light": "light.png",
      "dark": "dark.png",
      "size": 32,
    }],
    withDefaultIcon: false,
  });
});

add_task(async function browseraction_theme_icons_dynamic_theme() {
  let themeExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    files: {
      "background.png": BACKGROUND,
    },
    background() {
      browser.test.onMessage.addListener((msg, colors) => {
        if (msg === "update-theme") {
          browser.theme.update({
            "images": {
              "headerURL": "background.png",
            },
            "colors": colors,
          });
          browser.test.sendMessage("theme-updated");
        }
      });
    },
  });

  await themeExtension.startup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "browser_action": {
        "default_icon": "default.png",
        "theme_icons": [{
          "light": "light.png",
          "dark": "dark.png",
          "size": 19,
        }],
      },
    },
  });

  await extension.startup();

  // Confirm that the browser action has the default icon before a theme is set.
  await testBrowserAction(extension, "default.png");

  // Update the theme to a light theme.
  themeExtension.sendMessage("update-theme", LIGHT_THEME_COLORS);
  await themeExtension.awaitMessage("theme-updated");

  // Confirm that the dark icon is used for the light theme.
  await testBrowserAction(extension, "dark.png");

  // Update the theme to a dark theme.
  themeExtension.sendMessage("update-theme", DARK_THEME_COLORS);
  await themeExtension.awaitMessage("theme-updated");

  // Confirm that the light icon is used for the dark theme.
  await testBrowserAction(extension, "light.png");

  // Unload the theme.
  await themeExtension.unload();

  // Confirm that the default icon is used when the theme is unloaded.
  await testBrowserAction(extension, "default.png");

  await extension.unload();
});
