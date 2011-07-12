/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
onmessage = function(event) {
  var n = parseInt(event.data);

  if (n < 2) {
    postMessage(n);
    return;
  }

  var results = [];
  for (var i = 1; i <= 2; i++) {
    var worker = new Worker("fibonacci_worker.js");
    worker.onmessage = function(event) {
      results.push(parseInt(event.data));
      if (results.length == 2) {
        postMessage(results[0] + results[1]);
      }
    };
    worker.postMessage(n - i);
  }
}
