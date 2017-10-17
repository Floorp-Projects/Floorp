/*
 * Bug 1330882 - A test case for opening new windows through window.open() as
 *   rounded size when fingerprinting resistance is enabled. This test is for
 *   maximum values.
 */

OpenTest.run([
  {settingWidth: 1025, settingHeight: 1050, targetWidth: 1000, targetHeight: 1000},
  {settingWidth: 9999, settingHeight: 9999, targetWidth: 1000, targetHeight: 1000},
  {settingWidth: 999, settingHeight: 999, targetWidth: 1000, targetHeight: 1000}
], false);
