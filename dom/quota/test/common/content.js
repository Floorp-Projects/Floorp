/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_ERROR_STORAGE_BUSY = SpecialPowers.Cr.NS_ERROR_STORAGE_BUSY;

loadScript("dom/quota/test/common/global.js");

function clearAllDatabases(callback) {
  let qms = SpecialPowers.Services.qms;
  let principal = SpecialPowers.wrap(document).nodePrincipal;
  let request = qms.clearStoragesForPrincipal(principal);
  let cb = SpecialPowers.wrapCallback(callback);
  request.callback = cb;
  return request;
}

// SimpleDB connections and SpecialPowers wrapping:
//
// SpecialPowers provides a SpecialPowersHandler Proxy mechanism that lets our
// content-privileged code borrow its chrome-privileged principal to access
// things we shouldn't be able to access.  The proxies wrap their returned
// values, so once we have something wrapped we can rely on returned objects
// being wrapped as well.  The proxy will also automatically unwrap wrapped
// arguments we pass in.  However, we need to invoke wrapCallback on callback
// functions so that the arguments they receive will be wrapped because the
// proxy does not automatically wrap content-privileged functions.
//
// Our use of (wrapped) SpecialPowers.Cc results in getSimpleDatabase()
// producing a wrapped nsISDBConnection instance.  The nsISDBResult instances
// exposed on the (wrapped) nsISDBRequest are also wrapped.
// In particular, the wrapper takes responsibility for automatically cloning
// the ArrayBuffer returned by nsISDBResult.getAsArrayBuffer into the content
// compartment (rather than wrapping it) so that constructing a Uint8Array
// from it will succeed.

function getSimpleDatabase() {
  let connection = SpecialPowers.Cc[
    "@mozilla.org/dom/sdb-connection;1"
  ].createInstance(SpecialPowers.Ci.nsISDBConnection);

  let principal = SpecialPowers.wrap(document).nodePrincipal;

  connection.init(principal);

  return connection;
}

async function requestFinished(request) {
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
