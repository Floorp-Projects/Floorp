/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test verifies that the transport's queue operates correctly when various
 * packets are scheduled simultaneously.
 */

var { FileUtils } = Cu.import("resource://gre/modules/FileUtils.jsm", {});
var { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});

function run_test() {
  initTestDebuggerServer();

  add_task(function*() {
    yield test_transport(socket_transport);
    yield test_transport(local_transport);
    DebuggerServer.destroy();
  });

  run_next_test();
}

/*** Tests ***/

var test_transport = Task.async(function*(transportFactory) {
  let clientDeferred = promise.defer();
  let serverDeferred = promise.defer();

  // Ensure test files are not present from a failed run
  cleanup_files();
  let reallyLong = really_long();
  writeTestTempFile("bulk-input", reallyLong);

  do_check_eq(Object.keys(DebuggerServer._connections).length, 0);

  let transport = yield transportFactory();

  // Sending from client to server
  function write_data({copyFrom}) {
    NetUtil.asyncFetch({
      uri: NetUtil.newURI(getTestTempFile("bulk-input")),
      loadUsingSystemPrincipal: true
    }, function(input, status) {
        copyFrom(input).then(() => {
          input.close();
        });
      });
  }

  // Receiving on server from client
  function on_bulk_packet({actor, type, length, copyTo}) {
    do_check_eq(actor, "root");
    do_check_eq(type, "file-stream");
    do_check_eq(length, reallyLong.length);

    let outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    let output = FileUtils.openSafeFileOutputStream(outputFile);

    copyTo(output).then(() => {
      FileUtils.closeSafeFileOutputStream(output);
      return verify();
    }).then(() => {
      // It's now safe to close
      transport.hooks.onClosed = () => {
        clientDeferred.resolve();
      };
      transport.close();
    });
  }

  // Client

  function send_packets() {
    // Specifically, we want to ensure that multiple send()s proceed without
    // causing the transport to die.
    transport.send({
      actor: "root",
      type: "explode"
    });

    transport.startBulkSend({
       actor: "root",
       type: "file-stream",
       length: reallyLong.length
    }).then(write_data);
  }

  transport.hooks = {
    onPacket: function(packet) {
      if (packet.error) {
        transport.hooks.onError(packet);
      } else if (packet.applicationType) {
        transport.hooks.onServerHello(packet);
      } else {
        do_throw("Unexpected server reply");
      }
    },

    onServerHello: function(packet) {
      // We've received the initial start up packet
      do_check_eq(packet.from, "root");
      do_check_eq(packet.applicationType, "xpcshell-tests");

      // Server
      do_check_eq(Object.keys(DebuggerServer._connections).length, 1);
      do_print(Object.keys(DebuggerServer._connections));
      for (let connId in DebuggerServer._connections) {
        DebuggerServer._connections[connId].onBulkPacket = on_bulk_packet;
      }

      DebuggerServer.on("connectionchange", (event, type) => {
        if (type === "closed") {
          serverDeferred.resolve();
        }
      });

      send_packets();
    },

    onError: function(packet) {
      // The explode actor doesn't exist
      do_check_eq(packet.from, "root");
      do_check_eq(packet.error, "noSuchActor");
    },

    onClosed: function() {
      do_throw("Transport closed before we expected");
    }
  };

  transport.ready();

  return promise.all([clientDeferred.promise, serverDeferred.promise]);
});

/*** Test Utils ***/

function verify() {
  let reallyLong = really_long();

  let inputFile = getTestTempFile("bulk-input");
  let outputFile = getTestTempFile("bulk-output");

  do_check_eq(inputFile.fileSize, reallyLong.length);
  do_check_eq(outputFile.fileSize, reallyLong.length);

  // Ensure output file contents actually match
  let compareDeferred = promise.defer();
  NetUtil.asyncFetch({
    uri: NetUtil.newURI(getTestTempFile("bulk-output")),
    loadUsingSystemPrincipal: true
  }, input => {
      let outputData = NetUtil.readInputStreamToString(input, reallyLong.length);
      // Avoid do_check_eq here so we don't log the contents
      do_check_true(outputData === reallyLong);
      input.close();
      compareDeferred.resolve();
    });

  return compareDeferred.promise.then(cleanup_files);
}

function cleanup_files() {
  let inputFile = getTestTempFile("bulk-input", true);
  if (inputFile.exists()) {
    inputFile.remove(false);
  }

  let outputFile = getTestTempFile("bulk-output", true);
  if (outputFile.exists()) {
    outputFile.remove(false);
  }
}
