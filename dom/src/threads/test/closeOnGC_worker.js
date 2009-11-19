onclose = function() {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "closeOnGC_server.sjs", false);
  xhr.send();
};

postMessage("ready");
