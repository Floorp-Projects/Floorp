/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function readStream(inputStream) {
  let available = 0;
  let result = [];
  while ((available = inputStream.available()) > 0) {
    result.push(inputStream.readBytes(available));
  }
  return result.join("");
}

function handleRequest(request, response) {
  let rv = (() => {
    try {
      if (request.method != "POST") {
        return "ERROR: not a POST request";
      }

      let body = new URLSearchParams(
        readStream(new BinaryInputStream(request.bodyInputStream))
      );
      return body.get("payload").length;
    } catch (e) {
      return "ERROR: Exception: " + e;
    }
  })();

  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(`<!DOCTYPE HTML>
<script>
let rv = (${JSON.stringify(rv)});
opener.postMessage(rv, "*");
</script>
  `);
}
