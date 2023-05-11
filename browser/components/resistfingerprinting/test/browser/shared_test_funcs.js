function waitForMessage(aMsg, aOrigin) {
  return new Promise(resolve => {
    window.addEventListener("message", function listener(event) {
      if (event.data == aMsg && (aOrigin == "*" || event.origin == aOrigin)) {
        window.removeEventListener("message", listener);
        resolve();
      }
    });
  });
}
