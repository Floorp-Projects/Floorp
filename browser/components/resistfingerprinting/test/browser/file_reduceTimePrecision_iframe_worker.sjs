"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "application/javascript");

  let content = `onconnect = function(e) {
    let port = e.ports[0];

    let navigatorObj = self.navigator;
    let result = [];

    

    port.postMessage(result);
    port.start();
  };`;

  response.write(content);
}
