onconnect = function (e) {
  let port = e.ports[0];
  port.onmessage = function (m) {
    if (m.data === "RUN") {
      console.log("worker runs");
      port.postMessage("DONE");
      close();
    }
  };
};
