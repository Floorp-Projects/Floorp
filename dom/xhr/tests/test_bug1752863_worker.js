self.onmessage = async function(ev) {
  var xhr = new XMLHttpRequest({ mozAnon: false });
  var myself = self;
  xhr.addEventListener(
    "loadstart",
    async () => {
      try {
        xhr.open("POST", "FOOBAR", false);
        xhr.send();
      } catch (err) {
        if (err instanceof DOMException) {
          // This is what we expect to happen.
          myself.postMessage("DOMException");
        } else {
          myself.postMessage("OtherError");
        }
        // Let's ensure we still bail out from the processing.
        throw err;
      }
      // We do not expect to ever get here. However, this would happen
      // if the loadstart event would be queued and not directly
      // executed on the same C++ stack of the xhr.send syncloop.
      myself.postMessage("MissingError");
    },
    true
  );
  xhr.open("POST", "FOOBAR", false);
  xhr.send();
  // We do not expect to ever get here, see "MissingError".
  postMessage("TERMINATE");
};
