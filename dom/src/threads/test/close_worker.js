onclose = function() {
  postMessage("closed");
};

setTimeout(function() { close(); }, 1000);
