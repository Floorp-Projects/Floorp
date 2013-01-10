/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

self.onmessage = function onmessage() {
  self.postMessage({result: typeof OS == "undefined"});
};