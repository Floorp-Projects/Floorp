/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  if (event.data == 0) {
    postMessage(eval('40 + 2'));
  } else if (event.data > 0) {
    var worker = new Worker('csp_worker.js');
    worker.postMessage(event.data - 1);
  } else if (event.data == -1) {
    importScripts('data:application/javascript;base64,ZHVtcCgnaGVsbG8gd29ybGQnKQo=');
  } else if (event.data == -2) {
    importScripts('javascript:dump(123);');
  }
}
