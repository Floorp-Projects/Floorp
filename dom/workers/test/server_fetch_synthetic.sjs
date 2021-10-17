const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function log(str) {
  //dump(`SJS LOG: ${str}\n`);
}

/**
 * Given a multipart/form-data encoded string that we know to have only a single
 * part, return the contents of the part.  (MIME multipart encoding is too
 * exciting to delve into.)
 */
function extractBlobFromMultipartFormData(text) {
  const lines = text.split(/\r\n/g);
  const firstBlank = lines.indexOf("");
  const foo = lines.slice(firstBlank + 1, -2).join("\n");
  return foo;
}

async function handleRequest(request, response) {
  let blobContents = "";
  if (request.method !== "POST") {
  } else {
    var body = new BinaryInputStream(request.bodyInputStream);

    var avail;
    var bytes = [];

    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }
    let requestBodyContents = String.fromCharCode.apply(null, bytes);
    log(requestBodyContents);
    blobContents = extractBlobFromMultipartFormData(requestBodyContents);
  }

  log("Setting Headers");
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "200", "OK");
  response.write(`<!DOCTYPE HTML><head><meta charset="utf-8"/></head><body>
          <h1 id="url">${request.scheme}${request.host}${request.port}${request.path}</h1>
          <div id="source">ServerJS</div>
          <div id="blob">${blobContents}</div>
        </body>`);
  log("Done");
}
