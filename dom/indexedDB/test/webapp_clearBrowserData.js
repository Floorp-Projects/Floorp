/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const appDomain = "example.org";
const manifestURL =
  location.protocol + "//" + appDomain + "/manifest.webapp";

function testSteps()
{
  const objectStoreName = "foo";
  const testKey = 1;
  const testValue = objectStoreName;

  // Determine whether the app and browser frames should be in or
  // out-of-process.
  let remote_app, remote_browser;
  if (window.location.href.indexOf("inproc_oop") != -1) {
    remote_app = false;
    remote_browser = true;
  }
  else if (window.location.href.indexOf("oop_inproc") != -1) {
    remote_app = true;
    remote_browser = false;
  }
  else if (window.location.href.indexOf("inproc_inproc") != -1) {
    remote_app = false;
    remote_browser = false;
  }
  else {
    ok(false, "Bad test filename!");
    return;
  }

  let request = indexedDB.open(window.location.pathname, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = grabEventAndContinueHandler;
  request.onsuccess = unexpectedSuccessHandler;
  let event = yield undefined;

  let db = event.target.result;
  db.onerror = errorHandler;
  db.onversionchange = function(event) {
    event.target.close();
  }

  let objectStore = db.createObjectStore(objectStoreName);
  objectStore.add(testValue, testKey);

  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  // We need to send both remote_browser and remote_app in the querystring
  // because webapp_clearBrowserData_appFrame uses the path + querystring to
  // create and open a database which it checks no other test has touched.  If
  // we sent only remote_browser, then we wouldn't be able to test both
  // (remote_app==false, remote_browser==false) and (remote_app==true,
  // remote_browser==false).
  let srcURL = location.protocol + "//" + appDomain +
    location.pathname.substring(0, location.pathname.lastIndexOf('/')) +
    "/webapp_clearBrowserData_appFrame.html?" +
    "remote_browser=" + remote_browser + "&" +
    "remote_app=" + remote_app;

  let iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "");
  iframe.setAttribute("mozapp", manifestURL);
  iframe.setAttribute("src", srcURL);
  iframe.setAttribute("remote", remote_app);
  iframe.addEventListener("mozbrowsershowmodalprompt", function(event) {
    let message = JSON.parse(event.detail.message);
    switch (message.type) {
      case "info":
      case "ok":
        window[message.type].apply(window, message.args);
        break;
      case "done":
        continueToNextStepSync();
        break;
      default:
        throw "unknown message";
    }
  });

  info("loading app frame");

  document.body.appendChild(iframe);
  yield undefined;

  request = indexedDB.open(window.location.pathname, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = unexpectedSuccessHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  db = event.target.result;
  db.onerror = errorHandler;

  objectStore =
    db.transaction(objectStoreName).objectStore(objectStoreName);
  objectStore.get(testKey).onsuccess = grabEventAndContinueHandler;
  event = yield undefined;

  ok(testValue == event.target.result, "data still exists");

  finishTest();
  yield undefined;
}

function start()
{
  SpecialPowers.addPermission("browser", true, document);
  SpecialPowers.addPermission("browser", true, { manifestURL: manifestURL });
  SpecialPowers.addPermission("embed-apps", true, document);
  SpecialPowers.addPermission("indexedDB", true, { manifestURL: manifestURL });

  window.addEventListener("unload", function cleanup(event) {
    if (event.target == document) {
      window.removeEventListener("unload", cleanup, false);

      SpecialPowers.removePermission("browser", location.href);
      SpecialPowers.removePermission("browser",
                                     location.protocol + "//" + appDomain);
      SpecialPowers.removePermission("embed-apps", location.href);
      SpecialPowers.removePermission("indexedDB",
                                     location.protocol + "//" + appDomain);
    }
  }, false);

  SpecialPowers.pushPrefEnv({
    "set": [["dom.mozBrowserFramesEnabled", true]]
  }, runTest);
}
