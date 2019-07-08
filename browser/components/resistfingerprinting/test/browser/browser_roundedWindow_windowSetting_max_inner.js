/*
 * Bug 1330882 - A test case for setting window size through window.innerWidth/Height
 *   when fingerprinting resistance is enabled. This test is for maximum values.
 */

WindowSettingTest.run(
  [
    {
      settingWidth: 1025,
      settingHeight: 1050,
      targetWidth: 1000,
      targetHeight: 1000,
      initWidth: 200,
      initHeight: 100,
    },
    {
      settingWidth: 9999,
      settingHeight: 9999,
      targetWidth: 1000,
      targetHeight: 1000,
      initWidth: 200,
      initHeight: 100,
    },
    {
      settingWidth: 999,
      settingHeight: 999,
      targetWidth: 1000,
      targetHeight: 1000,
      initWidth: 200,
      initHeight: 100,
    },
  ],
  false
);
