/*
 * Bug 1330882 - A test case for setting window size through window.innerWidth/Height
 *   and window.outerWidth/Height when fingerprinting resistance is enabled. This
 *   test is for middle values.
 */

WindowSettingTest.run([
  {settingWidth: 600, settingHeight: 600, targetWidth: 600, targetHeight: 600, initWidth: 200, initHeight: 100},
  {settingWidth: 599, settingHeight: 599, targetWidth: 600, targetHeight: 600, initWidth: 200, initHeight: 100},
  {settingWidth: 401, settingHeight: 501, targetWidth: 600, targetHeight: 600, initWidth: 200, initHeight: 100}
]);
