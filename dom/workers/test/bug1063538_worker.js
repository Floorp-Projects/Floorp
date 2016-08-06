/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gJar = "jar:http://example.org/tests/dom/base/test/file_bug945152.jar!/data_big.txt";
var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});
var progressFired = false;

xhr.onloadend = function(e) {
  postMessage({type: 'finish', progressFired: progressFired });
  self.close();
};

xhr.onprogress = function(e) {
  if (e.loaded > 0) {
    progressFired = true;
  }
  xhr.abort();
};

onmessage = function(e) {
  xhr.open("GET", gJar, true);
  xhr.send();
}
