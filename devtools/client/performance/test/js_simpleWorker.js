self.addEventListener("message", function (e) {
  self.postMessage(e.data);
  self.close();
}, false);
