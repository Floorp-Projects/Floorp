onmessage = function(event) {
  var blob = event.data;

  blob.mozSlice(1, 5);

  postMessage("done");
}
