onmessage = function(event) {
  var xhr = new XMLHttpRequest();

  xhr.open("GET", event.data, false);
  xhr.send();
  xhr.open("GET", event.data, false);
  xhr.send();
  postMessage("done");
}
