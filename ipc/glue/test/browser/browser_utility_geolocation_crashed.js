/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function getGeolocation() {
  info("Requesting geolocation");

  let resolve;
  let promise = new Promise((_resolve, _reject) => {
    resolve = _resolve;
  });

  navigator.geolocation.getCurrentPosition(
    () => {
      ok(true, "geolocation succeeded");
      resolve(undefined);
    },
    () => {
      ok(false, "geolocation failed");
      resolve(undefined);
    }
  );

  return promise;
}

add_setup(async function () {
  // Avoid the permission doorhanger and cache that would trigger instead
  // of re-requesting location. Setting geo.timeout to 0 causes it to
  // retry the system geolocation (incl. recreating the utility process)
  // instead of reusing the MLS geolocation fallback it found the first time.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["geo.prompt.testing", true],
      ["geo.prompt.testing.allow", true],
      ["geo.provider.network.debug.requestCache.enabled", false],
      ["geo.provider.testing", false],
      ["geo.timeout", 0],
    ],
  });
});

add_task(async function testGeolocationProcessCrash() {
  info("Start the Windows utility process");
  await getGeolocation();

  info("Crash the utility process");
  await crashSomeUtilityActor("windowsUtils");

  info("Restart the Windows utility process");
  await getGeolocation();

  info("Confirm the restarted process");
  await checkUtilityExists("windowsUtils");

  info("Kill the utility process");
  await cleanUtilityProcessShutdown("windowsUtils", true);

  info("Restart the Windows utility process again");
  await getGeolocation();

  info("Confirm the restarted process");
  await checkUtilityExists("windowsUtils");
});

add_task(async function testCleanup() {
  info("Clean up to avoid confusing future tests");
  await cleanUtilityProcessShutdown("windowsUtils", true);
});
