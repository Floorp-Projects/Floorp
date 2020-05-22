/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const NS_ERROR_STORAGE_BUSY = SpecialPowers.Cr.NS_ERROR_STORAGE_BUSY;

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
// exposed on the (wrapped) nsISDBRequest are also wrapped, so our
// requestFinished helper wraps the results in helper objects that behave the
// same as the result, automatically unwrapping the wrapped array/arraybuffer
// results.

function getSimpleDatabase() {
  let connection = SpecialPowers.Cc[
    "@mozilla.org/dom/sdb-connection;1"
  ].createInstance(SpecialPowers.Ci.nsISDBConnection);

  let principal = SpecialPowers.wrap(document).nodePrincipal;

  connection.init(principal);

  return connection;
}

function requestFinished(request) {
  return new Promise(function(resolve, reject) {
    request.callback = SpecialPowers.wrapCallback(function(req) {
      if (req.resultCode === SpecialPowers.Cr.NS_OK) {
        let result = req.result;
        if (
          SpecialPowers.call_Instanceof(result, SpecialPowers.Ci.nsISDBResult)
        ) {
          let wrapper = {};
          for (let i in result) {
            if (typeof result[i] == "function") {
              wrapper[i] = SpecialPowers.unwrap(result[i]);
            } else {
              wrapper[i] = result[i];
            }
          }
          result = wrapper;
        }
        resolve(result);
      } else {
        reject(req.resultCode);
      }
    });
  });
}
