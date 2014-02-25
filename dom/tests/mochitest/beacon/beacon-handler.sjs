/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function DEBUG(str)
{
  // dump("********** " + str);
}

function handleRequest(request, response) {
  DEBUG("Entered request handler");
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.method == "GET") {
    response.setHeader("Content-Type", "application/json", false);
    switch (request.queryString) {
    case "getLastBeacon":
      var data = getState("beaconData");
      var mimetype = getState("beaconMimetype");
      DEBUG("GET was sending : " + data + "\n");
      DEBUG("GET was sending : " + mimetype + "\n");
      var result = {
        "data": data,
        "mimetype": mimetype,
      };
      response.write(JSON.stringify(result));
      break;
    default:
      response.setStatusLine(request.httpVersion, 400, "Bad Request");
      break;
    }
    return;
  }

  if (request.method == "POST") {
    var body = new BinaryInputStream(request.bodyInputStream);
    var avail;
    var bytes = [];

    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }

    var data = "";
    for (var i=0; i < bytes.length; i++) {
      // We are only passing strings at this point.
      if (bytes[i] < 32) continue;
      var charcode = String.fromCharCode(bytes[i]);
      data += charcode;
    }

    var mimetype = request.getHeader("Content-Type");

    // check to see if this is form data.
    if (mimetype.indexOf("multipart/form-data") != -1) {

      // trim the mime type to make testing easier.
      mimetype = "multipart/form-data";
      // Extract only the form-data name.

      var pattern = /; name=\"(.+)\";/;
      data = data.split(pattern)[1];
    }

    DEBUG("**********   POST was sending : " + data + "\n");
    DEBUG("**********   POST was sending : " + mimetype + "\n");
    setState("beaconData", data);
    setState("beaconMimetype", mimetype);

    response.setHeader("Content-Type", "text/plain", false);
    response.write('ok');
    return;
  }

  response.setStatusLine(request.httpVersion, 402, "Bad Request");
}
