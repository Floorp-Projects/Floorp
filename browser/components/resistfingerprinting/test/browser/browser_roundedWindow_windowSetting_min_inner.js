/*
 * Bug 1330882 - A test case for setting window size through window.innerWidth/Height
 *   when fingerprinting resistance is enabled. This test is for minimum values.
 */

WindowSettingTest.run(
  [
    {
      settingWidth: 199,
      settingHeight: 99,
      targetWidth: 200,
      targetHeight: 100,
      initWidth: 1000,
      initHeight: 1000,
    },
    {
      settingWidth: 10,
      settingHeight: 10,
      targetWidth: 200,
      targetHeight: 100,
      initWidth: 1000,
      initHeight: 1000,
    },
  ],
  false
);
