"use strict";

function handleRequest(request, response) {
  // The string "hello" repeated 10 times followed by newline. Compressed using gzip.
  // prettier-ignore
  let bytes = [0x1f, 0x8b, 0x08, 0x08, 0x4d, 0xe2, 0xf9, 0x54, 0x00, 0x03, 0x68,
               0x65, 0x6c, 0x6c, 0x6f, 0x00, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0xcf,
               0x20, 0x85, 0xe0, 0x02, 0x00, 0xf5, 0x4b, 0x38, 0xcf, 0x33, 0x00,
               0x00, 0x00];

  response.setHeader("Content-Encoding", "gzip", false);
  response.setHeader("Content-Length", "" + bytes.length, false);
  response.setHeader("Content-Type", "text/plain", false);

  let bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(response.bodyOutputStream);

  bos.writeByteArray(bytes);
}
