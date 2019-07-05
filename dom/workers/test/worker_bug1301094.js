onmessage = function(e) {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "worker_bug1301094.js", false);
  xhr.onload = function() {
    self.postMessage("OK");
  };

  var fd = new FormData();
  fd.append("file", e.data);
  xhr.send(fd);
};
