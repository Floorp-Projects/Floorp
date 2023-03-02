/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

function run_test() {
  let testDirName = do_get_cwd().clone();
  Services.env.set("MOZ_SYSTEM_CONFIG_DIR", testDirName.path);

  updateAppInfo();

  try {
    Services.dirsvc.undefine("SysConfD");
  } catch (e) {}
  let customSysConfD = Services.dirsvc.get("SysConfD", Ci.nsIFile);
  let parent = customSysConfD.parent;
  let child = customSysConfD.leafName;
  notEqual("/etc", parent.path, "SysConfD is not in /etc");
  equal("xpcshell", child, "SysConfD is xpcshell");
}
