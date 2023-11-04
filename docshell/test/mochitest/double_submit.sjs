"use strict";

let self = this;

let { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function log(str) {
  //  dump(`LOG: ${str}\n`);
}

function readStream(inputStream) {
  let available = 0;
  let result = [];
  while ((available = inputStream.available()) > 0) {
    result.push(inputStream.readBytes(available));
  }

  return result.join("");
}

function now() {
  return Date.now();
}

async function handleRequest(request, response) {
  log("Get query parameters");
  let params = new URLSearchParams(request.queryString);

  let start = now();
  let delay = parseInt(params.get("delay")) || 0;
  log(`Delay for ${delay}`);

  let message = "good";
  if (request.method !== "POST") {
    message = "bad";
  } else {
    log("Read POST body");
    let body = new URLSearchParams(
      readStream(new BinaryInputStream(request.bodyInputStream))
    );
    message = body.get("token") || "bad";
    log(`The result was ${message}`);
  }

  let body = `<!doctype html>
    <script>
    "use strict";
    let target = (opener || parent);
    target.postMessage(${JSON.stringify(message)}, '*');
    </script>`;

  // Sieze power from the response to allow manually transmitting data at any
  // rate we want, so we can delay transmitting headers.
  response.seizePower();

  log(`Writing HTTP status line at ${now() - start}`);
  response.write("HTTP/1.1 200 OK\r\n");

  await new Promise(resolve => setTimeout(() => resolve(), delay));

  log(`Delay completed at ${now() - start}`);
  response.write("Content-Type: text/html\r\n");
  response.write(`Content-Length: ${body.length}\r\n`);
  response.write("\r\n");
  response.write(body);
  response.finish();

  log("Finished");
}
