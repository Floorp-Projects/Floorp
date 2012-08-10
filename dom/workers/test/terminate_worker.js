/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onclose = function() {
  postMessage("Closed!");
}

onmessage = function(event) {
  throw "No messages should reach me!";
}

setInterval(function() { postMessage("Still alive!"); }, 100);
