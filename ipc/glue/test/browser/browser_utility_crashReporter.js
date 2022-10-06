/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function startAndCrashUtility(actors, actorsCheck) {
  const utilityPid = await startUtilityProcess(actors);
  await crashSomeUtility(utilityPid, actorsCheck);
}

// When running full suite, previous audio decoding tests might have left some
// running and this might interfere with our testing
add_setup(async function ensureNoExistingProcess() {
  await killPendingUtilityProcess();
});

add_task(async function utilityNoActor() {
  await startAndCrashUtility(0, actorNames => {
    return actorNames === undefined;
  });
});

add_task(async function utilityOneActor() {
  await startAndCrashUtility(1, actorNames => {
    return actorNames === kGenericUtilityActor;
  });
});

add_task(async function utilityManyActors() {
  await startAndCrashUtility(42, actorNames => {
    return (
      actorNames ===
      Array(42)
        .fill("unknown")
        .join(", ")
    );
  });
});
