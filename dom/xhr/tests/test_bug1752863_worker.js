var xhr;
var myself;

async function handleLoadstart() {
  try {
    xhr.open("POST", "FOOBAR", false);
    // This will potentially queue another "loadstart" event
    // before we can catch (err). But the order should be
    // guaranteed, that is the first postMessage arriving at
    // our parent is from the first catch (err).
    xhr.send();
    myself.postMessage("MissingError");
  } catch (err) {
    if (err instanceof DOMException) {
      // This is what we expect to happen on the first error
      // and the parent will check for this to arrive.
      myself.postMessage("DOMException");
    } else {
      myself.postMessage("OtherError");
    }
    // Let's ensure we still bail out from the processing.
    xhr.removeEventListener("loadstart", handleLoadstart, true);
    throw err;
  }
}

self.onmessage = async function () {
  xhr = new XMLHttpRequest({ mozAnon: false });
  myself = self;
  xhr.addEventListener("loadstart", handleLoadstart, true);
  xhr.open("POST", "FOOBAR", false);
  xhr.send();
  postMessage("TERMINATE");
};
