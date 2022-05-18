"use strict";

const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const BinaryOutputStream = CC(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);

function log(str) {
  // dump(`LOG: ${str}\n`);
}

async function handleRequest(request, response) {
  if (request.method !== "POST") {
    throw new Error("Expected a post request");
  } else {
    log("Reading request");
    let available = 0;
    let inputStream = new BinaryInputStream(request.bodyInputStream);
    while ((available = inputStream.available()) > 0) {
      log(inputStream.readBytes(available));
    }
  }

  log("Setting Headers");
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "200", "OK");
  log("Writing body");
  response.write(
    '<script>"use strict"; let target = opener ? opener : parent; target.postMessage("done", "*");</script>'
  );
  log("Done");
}
