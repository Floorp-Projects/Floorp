/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests for DevToolsUtils.fetch BOM detection.

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const BinaryOutputStream = Components.Constructor(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);

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

    const bos = new BinaryOutputStream(response.bodyOutputStream);
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

do_get_profile();

registerCleanupFunction(() => {
  return new Promise(resolve => server.stop(resolve));
});

add_task(async function () {
  await test_one(serverURL + "/u8", "UTF-8");
  await test_one(serverURL + "/u16be", "UTF-16BE");
  await test_one(serverURL + "/u16le", "UTF-16LE");
});

async function test_one(url, encoding) {
  // Be sure to set the encoding to something that will yield an
  // invalid result if BOM sniffing is not done.
  await DevToolsUtils.fetch(url, { charset: "ISO-8859-1" }).then(
    ({ content }) => {
      Assert.equal(content, "hı", "The content looks correct for " + encoding);
    }
  );
}
