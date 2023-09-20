/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

class RequestError extends Error {
  constructor(resultCode, resultName) {
    super(`Request failed (code: ${resultCode}, name: ${resultName})`);
    this.name = "RequestError";
    this.resultCode = resultCode;
    this.resultName = resultName;
  }
}

export function setStoragePrefs(optionalPrefsToSet) {
  const prefsToSet = [["dom.quotaManager.testing", true]];

  if (Services.appinfo.OS === "WINNT") {
    prefsToSet.push(["dom.quotaManager.useDOSDevicePathSyntax", true]);
  }

  if (optionalPrefsToSet) {
    prefsToSet.push(...optionalPrefsToSet);
  }

  for (const pref of prefsToSet) {
    Services.prefs.setBoolPref(pref[0], pref[1]);
  }
}

export function clearStoragePrefs(optionalPrefsToClear) {
  const prefsToClear = ["dom.quotaManager.testing", "dom.simpleDB.enabled"];

  if (Services.appinfo.OS === "WINNT") {
    prefsToClear.push("dom.quotaManager.useDOSDevicePathSyntax");
  }

  if (optionalPrefsToClear) {
    prefsToClear.push(...optionalPrefsToClear);
  }

  for (const pref of prefsToClear) {
    Services.prefs.clearUserPref(pref);
  }
}

export async function getUsageForOrigin(principal, fromMemory) {
  const request = Services.qms.getUsageForPrincipal(
    principal,
    function () {},
    fromMemory
  );

  await new Promise(function (resolve) {
    request.callback = function () {
      resolve();
    };
  });

  if (request.resultCode != Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}

export async function clearStoragesForOrigin(principal) {
  const request = Services.qms.clearStoragesForPrincipal(principal);

  await new Promise(function (resolve) {
    request.callback = function () {
      resolve();
    };
  });

  if (request.resultCode != Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}

export async function resetStorage() {
  const request = Services.qms.reset();

  await new Promise(function (resolve) {
    request.callback = function () {
      resolve();
    };
  });

  if (request.resultCode != Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}
