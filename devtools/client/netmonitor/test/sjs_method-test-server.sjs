/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  let body = "";
  if (request.method == "POST") {
    const bodyStream = new BinaryInputStream(request.bodyInputStream);

    let avail = 0;
    while ((avail = bodyStream.available()) > 0) {
      body += String.fromCharCode.apply(
        String,
        bodyStream.readByteArray(avail)
      );
    }
  }

  const contentType = request.hasHeader("content-type")
    ? request.getHeader("content-type")
    : "";

  const bodyOutput = [request.method, contentType, body].join("\n");
  response.bodyOutputStream.write(bodyOutput, bodyOutput.length);
}
