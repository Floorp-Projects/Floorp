/* eslint-env worker */

onconnect = function(evt) {
  evt.ports[0].onmessage = function(evt1) {
    var bc = new BroadcastChannel("foobar");
    bc.addEventListener("message", function(event) {
      bc.postMessage(
        event.data == "hello world from the window"
          ? "hello world from the worker"
          : "KO"
      );
      bc.close();
    });

    evt1.target.postMessage("READY");
  };
};
