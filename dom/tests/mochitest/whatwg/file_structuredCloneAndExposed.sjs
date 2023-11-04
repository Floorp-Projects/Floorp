"use strict";

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  response.setHeader("Content-Type", "application/javascript");

  let content = `function installListeners(input, target) {
    input.addEventListener("message", () => {
      target.postMessage(true, { targetOrigin: "*" });
    });
    input.addEventListener("messageerror", () => {
      target.postMessage(false, { targetOrigin: "*" });
    });
    target.postMessage("Inited", { targetOrigin: "*" });
  }

  ${query.get("additionalScript")}
  `;

  response.write(content);
}
