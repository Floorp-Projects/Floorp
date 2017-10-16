/*
 * Bug 1330882 - A test case for opening new windows through window.open() as
 *   rounded size when fingerprinting resistance is enabled. This test is for
 *   middle values.
 */

OpenTest.run([
  {settingWidth: 600, settingHeight: 600, targetWidth: 600, targetHeight: 600},
  {settingWidth: 599, settingHeight: 599, targetWidth: 600, targetHeight: 600},
  {settingWidth: 401, settingHeight: 501, targetWidth: 600, targetHeight: 600}
], true);
