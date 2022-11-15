"use strict";
const MR_TEMPLATE_PREF = "browser.aboutwelcome.templateMR";

async function openMRAboutWelcome() {
  await pushPrefs([MR_TEMPLATE_PREF, true]);
  await setAboutWelcomePref(true); // NB: Calls pushPrefs
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );

  return {
    browser: tab.linkedBrowser,
    cleanup: async () => {
      BrowserTestUtils.removeTab(tab);
      await popPrefs(); // for setAboutWelcomePref()
      await popPrefs(); // for pushPrefs()
    },
  };
}

add_task(async function test_aboutwelcome_video_content() {
  const TEST_CONTENT = [
    {
      id: "VIDEO_ONBOARDING",
      content: {
        position: "center",
        logo: {},
        title: "Video onboarding",
        secondary_button: {
          label: "Skip video",
          action: {
            navigate: true,
          },
        },
        video_container: {
          video_url: "",
          action: {
            navigate: true,
          },
          autoPlay: false,
        },
      },
    },
  ];
  await setAboutWelcomeMultiStage(JSON.stringify(TEST_CONTENT));
  let { cleanup, browser } = await openMRAboutWelcome();

  await test_screen_content(
    browser,
    "Video exists",
    ["main.with-video", "video[src='']"],
    []
  );
  await SpecialPowers.popPrefEnv();
  await cleanup();
});
