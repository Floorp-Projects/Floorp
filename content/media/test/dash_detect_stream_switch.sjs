/* -*- Mode: JavaScript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* dash_detect_stream_switch.sjs
 *
 * Parses requests for DASH manifests and ensures stream switching takes place
 * by verifying the subsegments downloaded and the streams they belong to.
 * If unexpected subsegments (byte ranges) are requested, the script will
 * will respond with a 404.
 */

var DEBUG = false;

function parseQuery(request, key) {
  var params = request.queryString.split('&');
  if (DEBUG) {
    dump("DASH-SJS: request params = \"" + params + "\"\n");
  }
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
	if (p == key)
	  return true;
    if (p.indexOf(key + "=") === 0)
	  return p.substring(key.length + 1);
	if (p.indexOf("=") < 0 && key === "")
	  return p;
  }
  return false;
}

function handleRequest(request, response)
{
  try {
    var name = parseQuery(request, "name");
    var range = request.hasHeader("Range") ? request.getHeader("Range")
                                           : undefined;
    
    // Should not get request for 1st subsegment from 2nd stream, nor 2nd
    // subsegment from 1st stream.
    if (name == "dash-webm-video-320x180.webm" && range == "bytes=25514-32767" ||
        name == "dash-webm-video-428x240.webm" && range == "bytes=228-35852") 
    {
      throw "Should not request " + name + " with byte-range " + range;
    } else {
      var rangeSplit = range.split("=");
      if (rangeSplit.length != 2) {
        throw "DASH-SJS: ERROR: invalid number of tokens (" + rangeSplit.length +
              ") delimited by \'=\' in \'Range\' header.";
      }
      var offsets = rangeSplit[1].split("-");
      if (offsets.length != 2) {
        throw "DASH-SJS: ERROR: invalid number of tokens (" + offsets.length +
              ") delimited by \'-\' in \'Range\' header.";
      }
      var startOffset = parseInt(offsets[0]);
      var endOffset = parseInt(offsets[1]);
      var file = Components.classes["@mozilla.org/file/directory_service;1"].
                            getService(Components.interfaces.nsIProperties).
                            get("CurWorkD", Components.interfaces.nsILocalFile);
      var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                            createInstance(Components.interfaces.nsIFileInputStream);
      var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                            createInstance(Components.interfaces.nsIBinaryInputStream);

      var paths = "tests/content/media/test/" + name;
      var split = paths.split("/");
      for (var i = 0; i < split.length; ++i) {
        file.append(split[i]);
      }

      fis.init(file, -1, -1, false);
      // Exception: start offset should be within file bounds.
      if (startOffset > file.fileSize) {
        throw "Starting offset [" + startOffset + "] is after end of file [" +
              file.fileSize + "].";
      }
      // End offset may be too large in the MPD. Real world HTTP servers just
      // return what data they can; do the same here - reduce the end offset.
      if (endOffset >= file.fileSize) {
        if (DEBUG) {
          dump("DASH-SJS: reducing endOffset [" + endOffset + "] to fileSize [" +
               (file.fileSize-1) + "]\n");
        }
        endOffset = file.fileSize-1;
      }
      fis.seek(Components.interfaces.nsISeekableStream.NS_SEEK_SET, startOffset);
      bis.setInputStream(fis);
      
      var byteLengthToRead = endOffset + 1 - startOffset;
      var totalBytesExpected = byteLengthToRead + startOffset;
      if (DEBUG) {
        dump("DASH-SJS: byteLengthToRead = " + byteLengthToRead +
             " byteLengthToRead+startOffset = " + totalBytesExpected +
             " fileSize = " + file.fileSize + "\n");
      }

      var bytes = bis.readBytes(byteLengthToRead);
      response.setStatusLine(request.httpVersion, 206, "Partial Content");
      response.setHeader("Content-Length", ""+bytes.length, false);
      response.setHeader("Content-Type", "application/dash+xml", false);
      var contentRange = "bytes " + startOffset + "-" + endOffset + "/" +
                         file.fileSize;
      response.setHeader("Content-Range", contentRange, false);
      response.write(bytes, bytes.length);
      bis.close();
    }
  } catch (e) {
    dump ("DASH-SJS-ERROR: " + e + "\n");
    response.setStatusLine(request.httpVersion, 404, "Not found");
  }
}
