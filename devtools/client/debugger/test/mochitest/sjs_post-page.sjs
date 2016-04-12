/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  let method = request.method;
  let body = "<script>\"" + method + "\";</script>";
  body += "<form method=\"POST\"><input type=\"submit\"></form>";
  response.bodyOutputStream.write(body, body.length);
}
