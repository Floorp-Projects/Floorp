"use strict";

let self = this;

Cu.import("resource://gre/modules/Timer.jsm");

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
//  dump(`LOG: ${str}\n`);
}

function* generateBody(fragments, size) {
  let result = [];
  let chunkSize = (size / fragments) | 0;
  let remaining = size;

  log(`Chunk size ${chunkSize}`)
  while (remaining > 0) {
    let data = new Uint8Array(Math.min(remaining, chunkSize));
    for (let i = 0; i < data.length; ++i) {
      // Generate a character in the [a-z] range.
      data[i] = 97 + Math.random() * (123 - 97);
    }

    yield data;
    log(`Remaining to chunk ${remaining}`)
    remaining -= data.length;
  }
}

function readStream(inputStream) {
  let available = 0;
  let result = [];
  while ((available = inputStream.available()) > 0) {
    result.push(inputStream.readBytes(available));
  }

  return result.join('');
}

function now() {
  return Date.now();
}

async function handleRequest(request, response) {
  log("Get query parameters");
  Cu.importGlobalProperties(["URLSearchParams"]);
  let params = new URLSearchParams(request.queryString);

  let delay = parseInt(params.get("delay")) || 0;
  let delayUntil = now() + delay;
  log(`Delay until ${delayUntil}`);

  let message = "good";
  if (request.method !== "POST") {
    message = "bad";
  } else {
    log("Read POST body");
    let body = new URLSearchParams(readStream(new BinaryInputStream(request.bodyInputStream)));
    message = body.get("token") || "bad";
    log(`The result was ${message}`);
  }

  let fragments = parseInt(params.get("fragments")) || 1;
  let size = parseInt(params.get("size")) || 1024;

  let outputStream = new BinaryOutputStream(response.bodyOutputStream);

  let header = "<!doctype html><!-- ";
  let footer = ` --><script>"use strict"; let target = (opener || parent); target.postMessage('${message}', '*');</script>`;

  log("Set headers")
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Content-Length", `${size + header.length + footer.length}`, false);
  response.setStatusLine(request.httpVersion, "200", "OK");

  response.processAsync();
  log("Write header");
  response.write(header);
  log("Write body")
  for (let data of generateBody(fragments, size)) {
    delay = Math.max(0, delayUntil - now())
    log(`Delay sending fragment for ${delay / fragments}`);
    let failed = false;
    await new Promise(resolve => {
      setTimeout(() => {
        try {
          outputStream.writeByteArray(data, data.length);
        } catch (e) {
          log(e.message);
          failed = true;
        }
        resolve();
      }, delay / fragments);
    });

    if (failed) {
      log("Stopped sending data");
      break;
    }

    fragments = Math.max(--fragments, 1);
    log(`Fragments left ${fragments}`)
  }

  log("Write footer")
  response.write(footer);

  response.finish();
}
