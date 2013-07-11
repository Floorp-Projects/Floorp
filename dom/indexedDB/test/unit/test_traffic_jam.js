/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function testSteps()
{
  const name = this.window ? window.location.pathname : "Splendid Test";

  let requests = [];
  function doOpen(version, errorCallback, upgradeNeededCallback, successCallback) {
    let request = indexedDB.open(name, version);
    request.onerror = errorCallback;
    request.onupgradeneeded = upgradeNeededCallback;
    request.onsuccess = successCallback;
    requests.push(request);
  }

  doOpen(1, errorHandler, grabEventAndContinueHandler, grabEventAndContinueHandler);
  doOpen(2, errorHandler, unexpectedSuccessHandler, unexpectedSuccessHandler);

  let event = yield undefined;
  is(event.type, "upgradeneeded", "expect an upgradeneeded event");
  is(event.target, requests[0], "fired at the right request");

  let db = event.target.result;
  db.createObjectStore("foo");

  doOpen(3, errorHandler, unexpectedSuccessHandler, unexpectedSuccessHandler);
  doOpen(2, errorHandler, unexpectedSuccessHandler, unexpectedSuccessHandler);
  doOpen(3, errorHandler, unexpectedSuccessHandler, unexpectedSuccessHandler);

  event.target.transaction.oncomplete = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "complete", "expect a complete event");
  is(event.target, requests[0].transaction, "expect it to be fired at the transaction");

  event = yield undefined;
  is(event.type, "success", "expect a success event");
  is(event.target, requests[0], "fired at the right request");
  event.target.result.close();

  requests[1].onupgradeneeded = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "upgradeneeded", "expect an upgradeneeded event");
  is(event.target, requests[1], "fired at the right request");

  requests[1].onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "expect a success event");
  is(event.target, requests[1], "fired at the right request");
  event.target.result.close();

  requests[2].onupgradeneeded = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "upgradeneeded", "expect an upgradeneeded event");
  is(event.target, requests[2], "fired at the right request");

  requests[2].onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "expect a success event");
  is(event.target, requests[2], "fired at the right request");
  event.target.result.close();

  requests[3].onerror = null;
  requests[3].addEventListener("error", new ExpectError("VersionError", true));

  event = yield undefined;

  requests[4].onsuccess = grabEventAndContinueHandler;

  event = yield undefined;
  is(event.type, "success", "expect a success event");
  is(event.target, requests[4], "fired at the right request");
  event.target.result.close();

  finishTest();
  yield undefined;
}

