/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onclose = function() {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "closeOnGC_server.sjs" + location.search, false);
  xhr.send();
};

postMessage("ready");
