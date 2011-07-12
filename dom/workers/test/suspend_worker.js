/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var counter = 0;

var interval = setInterval(function() {
  postMessage(++counter);
}, 100);

onmessage = function(event) {
  clearInterval(interval);
}
