/*
 * Bug 1330882 - A test case for opening new windows through window.open() as
 *   rounded size when fingerprinting resistance is enabled. This test is for
 *   minimum values.
 */

OpenTest.run([
  {settingWidth: 199, settingHeight: 99, targetWidth: 200, targetHeight: 100},
  {settingWidth: 10, settingHeight: 10, targetWidth: 200, targetHeight: 100}
], true);
