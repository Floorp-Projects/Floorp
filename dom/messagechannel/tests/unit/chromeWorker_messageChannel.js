/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

onmessage = function (pingEvt) {
  if (pingEvt.data == "ping") {
    let { port1, port2 } = new MessageChannel();
    port2.onmessage = helloEvt => {
      if (helloEvt.data == "hello") {
        helloEvt.ports[0].postMessage("goodbye");
      }
    };
    pingEvt.ports[0].postMessage("pong", [port1]);
  }
};
