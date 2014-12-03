onmessage = function() {
  var counter = 0;
  var id = setInterval(function() {
    ++counter;
    if (counter == 2) {
      clearInterval(id);
      postMessage('done');
    }
  }, 0);
}
