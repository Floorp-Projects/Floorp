/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function testThemeDeterminesToolbarQuery(toolbarProp) {
  let darkModeQuery = window.matchMedia("(-moz-system-dark-theme)");
  let darkToolbarQuery = window.matchMedia("(prefers-color-scheme: dark)");

  let darkToolbarExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "dark@mochi.test",
        },
      },
      theme: {
        colors: {
          [toolbarProp]: "#fff",
        },
      },
    },
  });
  let lightToolbarExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "light@mochi.test",
        },
      },
      theme: {
        colors: {
          [toolbarProp]: "#000",
        },
      },
    },
  });

  let darkToolbarLightContentExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "dark-light@mochi.test",
        },
      },
      theme: {
        colors: {
          [toolbarProp]: "#fff",
          ntp_text: "#000",
        },
      },
    },
  });

  let lightToolbarDarkContentExtension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "light-dark@mochi.test",
        },
      },
      theme: {
        colors: {
          [toolbarProp]: "#000",
          ntp_text: "#fff",
        },
      },
    },
  });

  let contentTab = await BrowserTestUtils.addTab(
    gBrowser,
    "http://example.com/browser/browser/components/extensions/test/browser/file_dummy.html"
  );
  await BrowserTestUtils.browserLoaded(contentTab.linkedBrowser);

  let preferencesTab = await BrowserTestUtils.addTab(
    gBrowser,
    "about:preferences"
  );
  await BrowserTestUtils.browserLoaded(preferencesTab.linkedBrowser);

  function queryMatchesInTab(tab) {
    gBrowser.selectedTab = tab;
    return SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
      // LookAndFeel updates are async.
      await new Promise(resolve => {
        content.requestAnimationFrame(() =>
          content.requestAnimationFrame(resolve)
        );
      });
      return content.matchMedia("(prefers-color-scheme: dark)").matches;
    });
  }

  function queryMatchesInContent() {
    return queryMatchesInTab(contentTab);
  }

  function queryMatchesInPreferences() {
    return queryMatchesInTab(preferencesTab);
  }

  async function testColors(
    expectedToolbarDark,
    expectedContentDark,
    description
  ) {
    is(
      darkToolbarQuery.matches,
      expectedToolbarDark,
      "toolbar query " + description
    );

    is(
      await queryMatchesInContent(),
      expectedContentDark,
      "content query " + description
    );
    is(
      await queryMatchesInPreferences(),
      expectedContentDark,
      "about:preferences query " + description
    );
  }

  let initialDarkModeMatches = darkModeQuery.matches;
  await testColors(initialDarkModeMatches, initialDarkModeMatches, "initially");

  async function testExtension(
    extension,
    expectedToolbarDark,
    expectedContentDark
  ) {
    await Promise.all([
      BrowserTestUtils.waitForEvent(darkToolbarQuery, "change"),
      TestUtils.topicObserved("lightweight-theme-styling-update"),
      TestUtils.topicObserved("look-and-feel-changed"),
      extension.startup(),
    ]);

    info("testing " + extension.id);

    is(darkModeQuery.matches, initialDarkModeMatches, "OS dark mode unchanged");
    await testColors(expectedToolbarDark, expectedContentDark, extension.id);

    info("tested " + extension.id);
  }

  let extensions = darkToolbarQuery.matches
    ? [
        lightToolbarExtension,
        darkToolbarExtension,
        lightToolbarDarkContentExtension,
        darkToolbarLightContentExtension,
      ]
    : [
        darkToolbarExtension,
        lightToolbarExtension,
        darkToolbarLightContentExtension,
        lightToolbarDarkContentExtension,
      ];

  await testExtension(
    extensions[0],
    !initialDarkModeMatches,
    !initialDarkModeMatches
  );
  await testExtension(
    extensions[1],
    initialDarkModeMatches,
    initialDarkModeMatches
  );
  await testExtension(
    extensions[2],
    !initialDarkModeMatches,
    initialDarkModeMatches
  );
  await testExtension(
    extensions[3],
    initialDarkModeMatches,
    !initialDarkModeMatches
  );

  for (let extension of extensions) {
    await extension.unload();
  }
  BrowserTestUtils.removeTab(contentTab);
  BrowserTestUtils.removeTab(preferencesTab);
}

add_task(function test_toolbar() {
  return testThemeDeterminesToolbarQuery("toolbar_text");
});

// Assert that we fall back to tab_background_text if toolbar is not set.
add_task(function test_frame() {
  return testThemeDeterminesToolbarQuery("tab_background_text");
});

const kDark = 0;
const kLight = 1;
const kDefault = 2;

// The above tests should be enough to make sure that the prefs behave as
// expected, the following ones test various edge cases in a simpler way.
async function testTheme(description, toolbar, content, themeData) {
  info(description);

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {
        gecko: {
          id: "dummy@mochi.test",
        },
      },
      theme: themeData,
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
    kDefault,
    {
      colors: {
        toolbar: "rgb(20, 17, 26)",
        toolbar_text: "rgb(251, 29, 78)",
      },
    }
  );

  // Dark frame text is ignored as it might be overlaid with an image,
  // see bug 1741931.
  await testTheme("Dark frame is ignored", kLight, kDefault, {
    colors: {
      frame: "#000000",
      tab_background_text: "#000000",
    },
  });

  await testTheme(
    "Semi-transparent toolbar backgrounds are ignored.",
    kLight,
    kDefault,
    {
      colors: {
        toolbar: "rgba(0, 0, 0, .2)",
        toolbar_text: "#000",
      },
    }
  );
});
