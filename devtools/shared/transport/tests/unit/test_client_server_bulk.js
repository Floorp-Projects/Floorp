/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {});
var Pipe = CC("@mozilla.org/pipe;1", "nsIPipe", "init");

function run_test() {
  initTestDebuggerServer();
  add_test_bulk_actor();

  add_task(async function() {
    await test_bulk_request_cs(socket_transport, "jsonReply", "json");
    await test_bulk_request_cs(local_transport, "jsonReply", "json");
    await test_bulk_request_cs(socket_transport, "bulkEcho", "bulk");
    await test_bulk_request_cs(local_transport, "bulkEcho", "bulk");
    await test_json_request_cs(socket_transport, "bulkReply", "bulk");
    await test_json_request_cs(local_transport, "bulkReply", "bulk");
    DebuggerServer.destroy();
  });

  run_next_test();
}

/** * Sample Bulk Actor ***/

function TestBulkActor(conn) {
  this.conn = conn;
}

TestBulkActor.prototype = {

  actorPrefix: "testBulk",

  bulkEcho: function({actor, type, length, copyTo}) {
    Assert.equal(length, really_long().length);
    this.conn.startBulkSend({
      actor: actor,
      type: type,
      length: length
    }).then(({copyFrom}) => {
      // We'll just echo back the same thing
      const pipe = new Pipe(true, true, 0, 0, null);
      copyTo(pipe.outputStream).then(() => {
        pipe.outputStream.close();
      });
      copyFrom(pipe.inputStream).then(() => {
        pipe.inputStream.close();
      });
    });
  },

  bulkReply: function({to, type}) {
    this.conn.startBulkSend({
      actor: to,
      type: type,
      length: really_long().length
    }).then(({copyFrom}) => {
      NetUtil.asyncFetch({
        uri: NetUtil.newURI(getTestTempFile("bulk-input")),
        loadUsingSystemPrincipal: true
      }, input => {
        copyFrom(input).then(() => {
          input.close();
        });
      });
    });
  },

  jsonReply: function({length, copyTo}) {
    Assert.equal(length, really_long().length);

    const outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    const output = FileUtils.openSafeFileOutputStream(outputFile);

    return copyTo(output).then(() => {
      FileUtils.closeSafeFileOutputStream(output);
      return verify_files();
    }).then(() => {
      return { allDone: true };
    }, do_throw);
  }

};

TestBulkActor.prototype.requestTypes = {
  "bulkEcho": TestBulkActor.prototype.bulkEcho,
  "bulkReply": TestBulkActor.prototype.bulkReply,
  "jsonReply": TestBulkActor.prototype.jsonReply
};

function add_test_bulk_actor() {
  DebuggerServer.addGlobalActor(TestBulkActor);
}

/** * Reply Handlers ***/

var replyHandlers = {

  json: function(request) {
    // Receive JSON reply from server
    const replyDeferred = defer();
    request.on("json-reply", (reply) => {
      Assert.ok(reply.allDone);
      replyDeferred.resolve();
    });
    return replyDeferred.promise;
  },

  bulk: function(request) {
    // Receive bulk data reply from server
    const replyDeferred = defer();
    request.on("bulk-reply", ({length, copyTo}) => {
      Assert.equal(length, really_long().length);

      const outputFile = getTestTempFile("bulk-output", true);
      outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

      const output = FileUtils.openSafeFileOutputStream(outputFile);

      copyTo(output).then(() => {
        FileUtils.closeSafeFileOutputStream(output);
        replyDeferred.resolve(verify_files());
      });
    });
    return replyDeferred.promise;
  }

};

/** * Tests ***/

var test_bulk_request_cs = async function(transportFactory, actorType, replyType) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  const clientDeferred = defer();
  const serverDeferred = defer();
  const bulkCopyDeferred = defer();

  const transport = await transportFactory();

  const client = new DebuggerClient(transport);
  client.connect().then(([app, traits]) => {
    Assert.equal(traits.bulk, true);
    client.listTabs().then(clientDeferred.resolve);
  });

  function bulkSendReadyCallback({copyFrom}) {
    NetUtil.asyncFetch({
      uri: NetUtil.newURI(getTestTempFile("bulk-input")),
      loadUsingSystemPrincipal: true
    }, input => {
      copyFrom(input).then(() => {
        input.close();
        bulkCopyDeferred.resolve();
      });
    });
  }

  clientDeferred.promise.then(response => {
    const request = client.startBulkRequest({
      actor: response.testBulk,
      type: actorType,
      length: really_long().length
    });

    // Send bulk data to server
    request.on("bulk-send-ready", bulkSendReadyCallback);

    // Set up reply handling for this type
    replyHandlers[replyType](request).then(() => {
      client.close();
      transport.close();
    });
  }).catch(do_throw);

  DebuggerServer.on("connectionchange", type => {
    if (type === "closed") {
      serverDeferred.resolve();
    }
  });

  return promise.all([
    clientDeferred.promise,
    bulkCopyDeferred.promise,
    serverDeferred.promise
  ]);
};

var test_json_request_cs = async function(transportFactory, actorType, replyType) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  const clientDeferred = defer();
  const serverDeferred = defer();

  const transport = await transportFactory();

  const client = new DebuggerClient(transport);
  client.connect((app, traits) => {
    Assert.equal(traits.bulk, true);
    client.listTabs().then(clientDeferred.resolve);
  });

  clientDeferred.promise.then(response => {
    const request = client.request({
      to: response.testBulk,
      type: actorType
    });

    // Set up reply handling for this type
    replyHandlers[replyType](request).then(() => {
      client.close();
      transport.close();
    });
  }).catch(do_throw);

  DebuggerServer.on("connectionchange", type => {
    if (type === "closed") {
      serverDeferred.resolve();
    }
  });

  return promise.all([
    clientDeferred.promise,
    serverDeferred.promise
  ]);
};

/** * Test Utils ***/

function verify_files() {
  const reallyLong = really_long();

  const inputFile = getTestTempFile("bulk-input");
  const outputFile = getTestTempFile("bulk-output");

  Assert.equal(inputFile.fileSize, reallyLong.length);
  Assert.equal(outputFile.fileSize, reallyLong.length);

  // Ensure output file contents actually match
  const compareDeferred = defer();
  NetUtil.asyncFetch({
    uri: NetUtil.newURI(getTestTempFile("bulk-output")),
    loadUsingSystemPrincipal: true
  }, input => {
    const outputData = NetUtil.readInputStreamToString(input, reallyLong.length);
      // Avoid do_check_eq here so we don't log the contents
    Assert.ok(outputData === reallyLong);
    input.close();
    compareDeferred.resolve();
  });

  return compareDeferred.promise.then(cleanup_files);
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
