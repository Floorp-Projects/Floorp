/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/content.js");

async function enableStorageTesting() {
  let prefsToSet = [
    ["dom.quotaManager.testing", true],
    ["dom.storageManager.enabled", true],
    ["dom.simpleDB.enabled", true],
  ];
  if (SpecialPowers.Services.appinfo.OS === "WINNT") {
    prefsToSet.push(["dom.quotaManager.useDOSDevicePathSyntax", true]);
  }

  await SpecialPowers.pushPrefEnv({ set: prefsToSet });
}
