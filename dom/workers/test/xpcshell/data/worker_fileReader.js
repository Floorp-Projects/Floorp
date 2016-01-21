/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */


self.onmessage = function(msg) {
  var fr = new FileReader();
  self.postMessage("OK");
};
