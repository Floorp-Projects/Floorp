/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kDark = 0;
const kLight = 1;
const kSystem = 2;

// The above tests should be enough to make sure that the prefs behave as
// expected, the following ones test various edge cases in a simpler way.
async function testTheme(description, toolbar, content, themeManifestData) {
  info(description);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_specific_settings: {
        gecko: {
          id: "dummy@mochi.test",
        },
      },
      ...themeManifestData,
    },
  });

  await Promise.all([
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    extension.startup(),
  ]);

  is(
    SpecialPowers.getIntPref("browser.theme.toolbar-theme"),
    toolbar,
    "Toolbar theme expected"
  );
  is(
    SpecialPowers.getIntPref("browser.theme.content-theme"),
    content,
    "Content theme expected"
  );

  await Promise.all([
    TestUtils.topicObserved("lightweight-theme-styling-update"),
    extension.unload(),
  ]);
}

add_task(async function test_dark_toolbar_dark_text() {
  // Bug 1743010
  await testTheme(
    "Dark toolbar color, dark toolbar background",
    kDark,
    kSystem,
    {
      theme: {
        colors: {
          toolbar: "rgb(20, 17, 26)",
          toolbar_text: "rgb(251, 29, 78)",
        },
      },
    }
  );

  // Dark frame text is ignored as it might be overlaid with an image,
  // see bug 1741931.
  await testTheme("Dark frame is ignored", kLight, kSystem, {
    theme: {
      colors: {
        frame: "#000000",
        tab_background_text: "#000000",
      },
    },
  });

  await testTheme(
    "Semi-transparent toolbar backgrounds are ignored.",
    kLight,
    kSystem,
    {
      theme: {
        colors: {
          toolbar: "rgba(0, 0, 0, .2)",
          toolbar_text: "#000",
        },
      },
    }
  );
});

add_task(async function dark_theme_presence_overrides_heuristics() {
  const systemScheme = window.matchMedia("(-moz-system-dark-theme)").matches
    ? kDark
    : kLight;
  await testTheme(
    "darkTheme presence overrides heuristics",
    systemScheme,
    kSystem,
    {
      theme: {
        colors: {
          toolbar: "#000",
          toolbar_text: "#fff",
        },
      },
      dark_theme: {
        colors: {
          toolbar: "#000",
          toolbar_text: "#fff",
        },
      },
    }
  );
});

add_task(async function color_scheme_override() {
  await testTheme(
    "color_scheme overrides toolbar / toolbar_text pair (dark)",
    kDark,
    kDark,
    {
      theme: {
        colors: {
          toolbar: "#fff",
          toolbar_text: "#000",
        },
        properties: {
          color_scheme: "dark",
        },
      },
    }
  );

  await testTheme(
    "color_scheme overrides toolbar / toolbar_text pair (light)",
    kLight,
    kLight,
    {
      theme: {
        colors: {
          toolbar: "#000",
          toolbar_text: "#fff",
        },
        properties: {
          color_scheme: "light",
        },
      },
    }
  );

  await testTheme(
    "content_color_scheme overrides ntp_text / ntp_background (dark)",
    kLight,
    kDark,
    {
      theme: {
        colors: {
          toolbar: "#fff",
          toolbar_text: "#000",
          ntp_background: "#fff",
          ntp_text: "#000",
        },
        properties: {
          content_color_scheme: "dark",
        },
      },
    }
  );

  await testTheme(
    "content_color_scheme overrides ntp_text / ntp_background (light)",
    kLight,
    kLight,
    {
      theme: {
        colors: {
          toolbar: "#fff",
          toolbar_text: "#000",
          ntp_background: "#000",
          ntp_text: "#fff",
        },
        properties: {
          content_color_scheme: "light",
        },
      },
    }
  );

  await testTheme(
    "content_color_scheme overrides color_scheme only for content",
    kLight,
    kDark,
    {
      theme: {
        colors: {
          toolbar: "#fff",
          toolbar_text: "#000",
          ntp_background: "#fff",
          ntp_text: "#000",
        },
        properties: {
          content_color_scheme: "dark",
        },
      },
    }
  );

  await testTheme(
    "content_color_scheme sytem overrides color_scheme only for content",
    kLight,
    kSystem,
    {
      theme: {
        colors: {
          toolbar: "#fff",
          toolbar_text: "#000",
          ntp_background: "#fff",
          ntp_text: "#000",
        },
        properties: {
          content_color_scheme: "system",
        },
      },
    }
  );

  await testTheme("color_scheme: sytem override", kSystem, kSystem, {
    theme: {
      colors: {
        toolbar: "#fff",
        toolbar_text: "#000",
        ntp_background: "#fff",
        ntp_text: "#000",
      },
      properties: {
        color_scheme: "system",
        content_color_scheme: "system",
      },
    },
  });
});

add_task(async function unified_theme() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.unified-color-scheme", true]],
  });

  await testTheme("Dark toolbar color", kDark, kDark, {
    theme: {
      colors: {
        toolbar: "rgb(20, 17, 26)",
        toolbar_text: "rgb(251, 29, 78)",
      },
    },
  });

  await testTheme("Light toolbar color", kLight, kLight, {
    theme: {
      colors: {
        toolbar: "white",
        toolbar_text: "black",
      },
    },
  });

  await SpecialPowers.popPrefEnv();
});
