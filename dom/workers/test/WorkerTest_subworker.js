/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  let chromeURL = event.data.replace(
    "test_chromeWorkerJSM.xhtml",
    "WorkerTest_badworker.js"
  );

  let mochitestURL = event.data
    .replace("test_chromeWorkerJSM.xhtml", "WorkerTest_badworker.js")
    .replace(
      "chrome://mochitests/content/chrome",
      "http://mochi.test:8888/tests"
    );

  // We should be able to XHR to anything we want, including a chrome URL.
  let xhr = new XMLHttpRequest();
  xhr.open("GET", mochitestURL, false);
  xhr.send();

  if (!xhr.responseText) {
    throw "Can't load script file via XHR!";
  }

  // We shouldn't be able to make a ChromeWorker to a non-chrome URL.
  try {
    new ChromeWorker(mochitestURL);
  } catch (e) {
    if (e.name === "SecurityError") {
      postMessage("Done");
      return;
    }
  }
  throw "creating a chrome worker with a bad URL should throw a SecurityError";
};
