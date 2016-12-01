// Worker that errors on receiving an install event.
oninstall = function(e) {
  e.waitUntil( new Promise(function(resolve, reject) {
    undefined.doSomething;
    resolve();
  }));
};
