/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

loadScript("dom/quota/test/common/system.js");

function addTest(testFunction) {
  const taskFunction = async function () {
    await enableStorageTesting();

    await testFunction();
  };

  Object.defineProperty(taskFunction, "name", {
    value: testFunction.name,
    writable: false,
  });

  add_task(taskFunction);
}

async function enableStorageTesting() {
  const prefsToSet = [
    ["dom.quotaManager.testing", true],
    ["dom.simpleDB.enabled", true],
  ];
  if (Services.appinfo.OS === "WINNT") {
    prefsToSet.push(["dom.quotaManager.useDOSDevicePathSyntax", true]);
  }

  await SpecialPowers.pushPrefEnv({ set: prefsToSet });
}
