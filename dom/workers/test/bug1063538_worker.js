/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var gJar = "jar:http://example.org/tests/content/base/test/file_bug945152.jar!/data_big.txt";
var xhr = new XMLHttpRequest({mozAnon: true, mozSystem: true});

xhr.onprogress = function(e) {
  xhr.abort();
  postMessage({type: 'finish' });
  self.close();
};

onmessage = function(e) {
  xhr.open("GET", gJar, true);
  xhr.send();
}
