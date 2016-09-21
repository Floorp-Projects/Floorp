/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch BOM detection.

const CC = Components.Constructor;

const { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
const BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                              "nsIBinaryOutputStream", "setOutputStream");

function write8(bos) {
  bos.write8(0xef);
  bos.write8(0xbb);
  bos.write8(0xbf);
  bos.write8(0x68);
  bos.write8(0xc4);
  bos.write8(0xb1);
}

function write16be(bos) {
  bos.write8(0xfe);
  bos.write8(0xff);
  bos.write8(0x00);
  bos.write8(0x68);
  bos.write8(0x01);
  bos.write8(0x31);
}

function write16le(bos) {
  bos.write8(0xff);
  bos.write8(0xfe);
  bos.write8(0x68);
  bos.write8(0x00);
  bos.write8(0x31);
  bos.write8(0x01);
}

function getHandler(writer) {
  return function (request, response) {
    response.setStatusLine(request.httpVersion, 200, "OK");

    let bos = new BinaryOutputStream(response.bodyOutputStream);
    writer(bos);
  };
}

const server = new HttpServer();
server.registerDirectory("/", do_get_cwd());
server.registerPathHandler("/u8", getHandler(write8));
server.registerPathHandler("/u16be", getHandler(write16be));
server.registerPathHandler("/u16le", getHandler(write16le));
server.start(-1);

const port = server.identity.primaryPort;
const serverURL = "http://localhost:" + port;

do_register_cleanup(() => {
  return new Promise(resolve => server.stop(resolve));
});

add_task(function* () {
  yield test_one(serverURL + "/u8", "UTF-8");
  yield test_one(serverURL + "/u16be", "UTF-16BE");
  yield test_one(serverURL + "/u16le", "UTF-16LE");
});

function* test_one(url, encoding) {
  // Be sure to set the encoding to something that will yield an
  // invalid result if BOM sniffing is not done.
  yield DevToolsUtils.fetch(url, { charset: "ISO-8859-1" }).then(({content}) => {
    do_check_eq(content, "hı", "The content looks correct for " + encoding);
  });
}
