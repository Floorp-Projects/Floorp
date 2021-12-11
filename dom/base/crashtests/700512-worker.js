onmessage = function(event) {
  var blob = event.data;

  blob.slice(1, 5);

  postMessage("done");
}
