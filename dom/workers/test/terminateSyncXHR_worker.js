/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

onmessage = function(event) {
  throw "No messages should reach me!";
}

var xhr = new XMLHttpRequest();
xhr.open("GET", "testXHR.txt", false);
xhr.addEventListener("loadstart", function ()
{
  // Tell the parent to terminate us.
  postMessage("TERMINATE");
  // And wait for it to do so.
  while(1) { true; }
});
xhr.send(null);
