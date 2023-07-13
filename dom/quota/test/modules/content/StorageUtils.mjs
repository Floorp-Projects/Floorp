/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This file expectes the SpecialPowers to be available in the scope
// it is loaded into.
/* global SpecialPowers */

class RequestError extends Error {
  constructor(resultCode, resultName) {
    super(`Request failed (code: ${resultCode}, name: ${resultName})`);
    this.name = "RequestError";
    this.resultCode = resultCode;
    this.resultName = resultName;
  }
}

export async function setStoragePrefs(optionalPrefsToSet) {
  const prefsToSet = [
    // Not needed right now, but might be needed in future.
    // ["dom.quotaManager.testing", true],
  ];

  if (SpecialPowers.Services.appinfo.OS === "WINNT") {
    prefsToSet.push(["dom.quotaManager.useDOSDevicePathSyntax", true]);
  }

  if (optionalPrefsToSet) {
    prefsToSet.push(...optionalPrefsToSet);
  }

  await SpecialPowers.pushPrefEnv({ set: prefsToSet });
}

export async function getUsageForOrigin(principal, fromMemory) {
  const request = SpecialPowers.Services.qms.getUsageForPrincipal(
    principal,
    function () {},
    fromMemory
  );

  await new Promise(function (resolve) {
    request.callback = SpecialPowers.wrapCallback(function () {
      resolve();
    });
  });

  if (request.resultCode != SpecialPowers.Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}

export async function clearStoragesForOrigin(principal) {
  const request =
    SpecialPowers.Services.qms.clearStoragesForPrincipal(principal);

  await new Promise(function (resolve) {
    request.callback = SpecialPowers.wrapCallback(function () {
      resolve();
    });
  });

  if (request.resultCode != SpecialPowers.Cr.NS_OK) {
    throw new RequestError(request.resultCode, request.resultName);
  }

  return request.result;
}
