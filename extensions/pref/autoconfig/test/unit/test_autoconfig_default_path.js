/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { updateAppInfo } = ChromeUtils.import(
  "resource://testing-common/AppInfo.jsm"
);

function run_test() {
  updateAppInfo();

  let defaultSysConfD = Services.dirsvc.get("SysConfD", Ci.nsIFile);
  equal("/etc/xpcshell", defaultSysConfD.path, "SysConfD is in /etc");
}
