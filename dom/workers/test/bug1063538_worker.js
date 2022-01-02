/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gURL = "http://example.org/tests/dom/workers/test/bug1063538.sjs";
var xhr = new XMLHttpRequest({ mozAnon: true, mozSystem: true });
var progressFired = false;

xhr.onloadend = function(e) {
  postMessage({ type: "finish", progressFired });
  self.close();
};

xhr.onprogress = function(e) {
  if (e.loaded > 0) {
    progressFired = true;
    xhr.abort();
  }
};

onmessage = function(e) {
  xhr.open("GET", gURL, true);
  xhr.send();
};
