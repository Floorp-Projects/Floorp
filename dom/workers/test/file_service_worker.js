self.onmessage = evt => {
  evt.ports[0].postMessage("serviceworker-reply");
};
