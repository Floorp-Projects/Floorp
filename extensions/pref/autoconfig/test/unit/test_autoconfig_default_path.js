/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

function run_test() {
  updateAppInfo();

  try {
    Services.dirsvc.undefine("SysConfD");
  } catch (e) {}
  let defaultSysConfD = Services.dirsvc.get("SysConfD", Ci.nsIFile);
  equal("/etc/xpcshell", defaultSysConfD.path, "SysConfD is in /etc");
}
