/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");

function run_test() {
  initTestDevToolsServer();

  add_task(async function() {
    await test_bulk_transfer_transport(socket_transport);
    await test_bulk_transfer_transport(local_transport);
    DevToolsServer.destroy();
  });

  run_next_test();
}

/** * Tests ***/

/**
 * This tests a one-way bulk transfer at the transport layer.
 */
var test_bulk_transfer_transport = async function(transportFactory) {
  info("Starting bulk transfer test at " + new Date().toTimeString());

  let clientResolve;
  const clientDeferred = new Promise(resolve => {
    clientResolve = resolve;
  });

  let serverResolve;
  const serverDeferred = new Promise(resolve => {
    serverResolve = resolve;
  });

  // Ensure test files are not present from a failed run
  cleanup_files();
  const reallyLong = really_long();
  writeTestTempFile("bulk-input", reallyLong);

  Assert.equal(Object.keys(DevToolsServer._connections).length, 0);

  const transport = await transportFactory();

  // Sending from client to server
  function write_data({ copyFrom }) {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(getTestTempFile("bulk-input")),
        loadUsingSystemPrincipal: true,
      },
      function(input, status) {
        copyFrom(input).then(() => {
          input.close();
        });
      }
    );
  }

  // Receiving on server from client
  function on_bulk_packet({ actor, type, length, copyTo }) {
    Assert.equal(actor, "root");
    Assert.equal(type, "file-stream");
    Assert.equal(length, reallyLong.length);

    const outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    const output = FileUtils.openSafeFileOutputStream(outputFile);

    copyTo(output)
      .then(() => {
        FileUtils.closeSafeFileOutputStream(output);
        return verify();
      })
      .then(() => {
        // It's now safe to close
        transport.hooks.onTransportClosed = () => {
          clientResolve();
        };
        transport.close();
      });
  }

  // Client
  transport.hooks = {
    onPacket(packet) {
      // We've received the initial start up packet
      Assert.equal(packet.from, "root");

      // Server
      Assert.equal(Object.keys(DevToolsServer._connections).length, 1);
      info(Object.keys(DevToolsServer._connections));
      for (const connId in DevToolsServer._connections) {
        DevToolsServer._connections[connId].onBulkPacket = on_bulk_packet;
      }

      DevToolsServer.on("connectionchange", type => {
        if (type === "closed") {
          serverResolve();
        }
      });

      transport
        .startBulkSend({
          actor: "root",
          type: "file-stream",
          length: reallyLong.length,
        })
        .then(write_data);
    },

    onTransportClosed() {
      do_throw("Transport closed before we expected");
    },
  };

  transport.ready();

  return Promise.all([clientDeferred, serverDeferred]);
};

/** * Test Utils ***/

function verify() {
  const reallyLong = really_long();

  const inputFile = getTestTempFile("bulk-input");
  const outputFile = getTestTempFile("bulk-output");

  Assert.equal(inputFile.fileSize, reallyLong.length);
  Assert.equal(outputFile.fileSize, reallyLong.length);

  // Ensure output file contents actually match
  return new Promise(resolve => {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(getTestTempFile("bulk-output")),
        loadUsingSystemPrincipal: true,
      },
      input => {
        const outputData = NetUtil.readInputStreamToString(
          input,
          reallyLong.length
        );
        // Avoid do_check_eq here so we don't log the contents
        Assert.ok(outputData === reallyLong);
        input.close();
        resolve();
      }
    );
  }).then(cleanup_files);
}

function cleanup_files() {
  const inputFile = getTestTempFile("bulk-input", true);
  if (inputFile.exists()) {
    inputFile.remove(false);
  }

  const outputFile = getTestTempFile("bulk-output", true);
  if (outputFile.exists()) {
    outputFile.remove(false);
  }
}
