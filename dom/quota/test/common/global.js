/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const INT64_MIN = -0x8000000000000000n;

class RequestError extends Error {
  constructor(resultCode, resultName) {
    super(`Request failed (code: ${resultCode}, name: ${resultName})`);
    this.name = "RequestError";
    this.resultCode = resultCode;
    this.resultName = resultName;
  }
}

function openDBRequestUpgradeNeeded(request) {
  return new Promise(function (resolve, reject) {
    request.onerror = function (event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onupgradeneeded = function (event) {
      resolve(event);
    };
    request.onsuccess = function (event) {
      ok(false, "Got success, but did not expect it!");
      reject(event);
    };
  });
}

function openDBRequestSucceeded(request) {
  return new Promise(function (resolve, reject) {
    request.onerror = function (event) {
      ok(false, "indexedDB error, '" + event.target.error.name + "'");
      reject(event);
    };
    request.onupgradeneeded = function (event) {
      ok(false, "Got upgrade, but did not expect it!");
      reject(event);
    };
    request.onsuccess = function (event) {
      resolve(event);
    };
  });
}
