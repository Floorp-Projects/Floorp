onmessage = function(evt) {
  evt.data.onmessage = function(event) {
    evt.data.postMessage(event.data);
  }
}

postMessage("READY");
