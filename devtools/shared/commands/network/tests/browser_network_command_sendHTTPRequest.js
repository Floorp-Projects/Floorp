/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the NetworkCommand's sendHTTPRequest

add_task(async function () {
  info("Test NetworkCommand.sendHTTPRequest");
  const tab = await addTab("data:text/html,foo");
  const commands = await CommandsFactory.forTab(tab);

  // We have to ensure TargetCommand is initialized to have access to the top level target
  // from NetworkCommand.sendHTTPRequest
  await commands.targetCommand.startListening();

  const { networkCommand } = commands;

  const httpServer = createTestHTTPServer();
  const onRequest = new Promise(resolve => {
    httpServer.registerPathHandler(
      "/http-request.html",
      (request, response) => {
        response.setStatusLine(request.httpVersion, 200, "OK");
        response.write("Response body");
        resolve(request);
      }
    );
  });
  const url = `http://localhost:${httpServer.identity.primaryPort}/http-request.html`;

  info("Call NetworkCommand.sendHTTPRequest");
  const { resourceCommand } = commands;
  const { onResource } = await resourceCommand.waitForNextResource(
    resourceCommand.TYPES.NETWORK_EVENT
  );
  const { channelId } = await networkCommand.sendHTTPRequest({
    url,
    method: "POST",
    headers: [{ name: "Request", value: "Header" }],
    body: "Hello",
    cause: {
      loadingDocumentUri: "https://example.com",
      stacktraceAvailable: true,
      type: "xhr",
    },
  });
  ok(channelId, "Received a channel id in response");
  const resource = await onResource;
  is(
    resource.resourceId,
    channelId,
    "NETWORK_EVENT resource channelId is the same as the one returned by sendHTTPRequest"
  );

  const request = await onRequest;
  is(request.method, "POST", "Request method is correct");
  is(request.getHeader("Request"), "Header", "The custom header was passed");
  is(fetchRequestBody(request), "Hello", "The request POST's body is correct");

  await commands.destroy();
});

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function fetchRequestBody(request) {
  let body = "";
  const bodyStream = new BinaryInputStream(request.bodyInputStream);
  let avail = 0;
  while ((avail = bodyStream.available()) > 0) {
    body += String.fromCharCode.apply(String, bodyStream.readByteArray(avail));
  }
  return body;
}
