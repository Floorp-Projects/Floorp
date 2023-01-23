/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const LIGHT_THEME_COLORS = {
  frame: "#FFF",
  tab_background_text: "#000",
};

const DARK_THEME_COLORS = {
  frame: "#000",
  tab_background_text: "#FFF",
};

const TOOLBAR_MAPPING = {
  navbar: "nav-bar",
  tabstrip: "TabsToolbar",
};

async function testBrowserAction(extension, expectedIcon) {
  let browserActionWidget = getBrowserActionWidget(extension);
  await promiseAnimationFrame();
  let browserActionButton = browserActionWidget.forWindow(window).node;
  let image = getListStyleImage(browserActionButton.firstElementChild);
  ok(
    image.includes(expectedIcon),
    `Expected browser action icon (${image}) to be ${expectedIcon}`
  );
}

async function testStaticTheme(options) {
  let {
    themeData,
    themeIcons,
    withDefaultIcon,
    expectedIcon,
    defaultArea = "navbar",
  } = options;

  let manifest = {
    browser_action: {
      theme_icons: themeIcons,
      default_area: defaultArea,
    },
  };

  if (withDefaultIcon) {
    manifest.browser_action.default_icon = "default.png";
  }

  let extension = ExtensionTestUtils.loadExtension({ manifest });

  await extension.startup();

  // Ensure we show the menupanel at least once. This makes sure that the
  // elements we're going to query the style of are in the flat tree.
  if (defaultArea == "menupanel") {
    let shown = BrowserTestUtils.waitForPopupEvent(
      window.gUnifiedExtensions.panel,
      "shown"
    );
    window.gUnifiedExtensions.togglePanel();
    await shown;
  }

  // Confirm that the browser action has the correct default icon before a theme is loaded.
  let toolbarId = TOOLBAR_MAPPING[defaultArea];
  let expectedDefaultIcon;
  // Some platforms have dark toolbars by default, take it in account when picking the default icon.
  if (
    toolbarId &&
    document.getElementById(toolbarId).hasAttribute("brighttext")
  ) {
    expectedDefaultIcon = "light.png";
  } else {
    expectedDefaultIcon = withDefaultIcon ? "default.png" : "dark.png";
  }
  await testBrowserAction(extension, expectedDefaultIcon);

  let theme = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: themeData,
      },
    },
  });

  await theme.startup();

  // Confirm that the correct icon is used when the theme is loaded.
  if (expectedIcon == "dark") {
    // The dark icon should be used if the area is light.
    await testBrowserAction(extension, "dark.png");
  } else {
    // The light icon should be used if the area is dark.
    await testBrowserAction(extension, "light.png");
  }

  await theme.unload();

  // Confirm that the browser action has the correct default icon when the theme is unloaded.
  await testBrowserAction(extension, expectedDefaultIcon);

  await extension.unload();
}

add_task(async function browseraction_theme_icons_light_theme() {
  await testStaticTheme({
    themeData: LIGHT_THEME_COLORS,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData: LIGHT_THEME_COLORS,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
    withDefaultIcon: false,
  });
});

add_task(async function browseraction_theme_icons_dark_theme() {
  await testStaticTheme({
    themeData: DARK_THEME_COLORS,
    expectedIcon: "light",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData: DARK_THEME_COLORS,
    expectedIcon: "light",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
    withDefaultIcon: false,
  });
});

add_task(async function browseraction_theme_icons_different_toolbars() {
  let themeData = {
    frame: "#000",
    tab_background_text: "#fff",
    toolbar: "#fff",
    bookmark_text: "#000",
  };
  await testStaticTheme({
    themeData,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
  });
  await testStaticTheme({
    themeData,
    expectedIcon: "light",
    defaultArea: "tabstrip",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData,
    expectedIcon: "light",
    defaultArea: "tabstrip",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
  });
});

add_task(async function browseraction_theme_icons_overflow_panel() {
  let themeData = {
    popup: "#000",
    popup_text: "#fff",
  };
  await testStaticTheme({
    themeData,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData,
    expectedIcon: "dark",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
  });

  await testStaticTheme({
    themeData,
    expectedIcon: "light",
    defaultArea: "menupanel",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 19,
      },
    ],
    withDefaultIcon: true,
  });
  await testStaticTheme({
    themeData,
    expectedIcon: "light",
    defaultArea: "menupanel",
    themeIcons: [
      {
        light: "light.png",
        dark: "dark.png",
        size: 16,
      },
      {
        light: "light.png",
        dark: "dark.png",
        size: 32,
      },
    ],
  });
});

add_task(async function browseraction_theme_icons_dynamic_theme() {
  let themeExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["theme"],
    },
    background() {
      browser.test.onMessage.addListener((msg, colors) => {
        if (msg === "update-theme") {
          browser.theme.update({
            colors: colors,
          });
          browser.test.sendMessage("theme-updated");
        }
      });
    },
  });

  await themeExtension.startup();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {
        default_icon: "default.png",
        default_area: "navbar",
        theme_icons: [
          {
            light: "light.png",
            dark: "dark.png",
            size: 16,
          },
          {
            light: "light.png",
            dark: "dark.png",
            size: 32,
          },
        ],
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
