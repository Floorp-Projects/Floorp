var port;
onconnect = function(evt) {
  evt.source.postMessage({ type: "connected" });

  if (!port) {
    port = evt.source;
    evt.source.onmessage = function(evtFromPort) {
      port.postMessage({type: "status",
                        test: "Port from the main-thread!" == evtFromPort.data,
                        msg: "The message is coming from the main-thread"});
      port.postMessage({type: "status",
                        test: (evtFromPort.ports.length == 1),
                        msg: "1 port transferred"});

      evtFromPort.ports[0].onmessage = function(evtFromPort2) {
        port.postMessage({type: "status",
                          test: (evtFromPort2.data.type == "connected"),
                          msg: "The original message received" });
        port.postMessage({type: "finish"});
        close();
      }
    }
  }
}
