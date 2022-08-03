/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var { FileUtils } = ChromeUtils.import("resource://gre/modules/FileUtils.jsm");
var Pipe = CC("@mozilla.org/pipe;1", "nsIPipe", "init");

function run_test() {
  initTestDevToolsServer();
  add_test_bulk_actor();

  add_task(async function() {
    await test_bulk_request_cs(socket_transport, "jsonReply", "json");
    await test_bulk_request_cs(local_transport, "jsonReply", "json");
    await test_bulk_request_cs(socket_transport, "bulkEcho", "bulk");
    await test_bulk_request_cs(local_transport, "bulkEcho", "bulk");
    await test_json_request_cs(socket_transport, "bulkReply", "bulk");
    await test_json_request_cs(local_transport, "bulkReply", "bulk");
    DevToolsServer.destroy();
  });

  run_next_test();
}

/** * Sample Bulk Actor ***/
const { Actor } = require("devtools/shared/protocol/Actor");
class TestBulkActor extends Actor {
  constructor(conn) {
    super(conn);

    this.typeName = "testBulk";
    this.requestTypes = {
      bulkEcho: this.bulkEcho,
      bulkReply: this.bulkReply,
      jsonReply: this.jsonReply,
    };
  }

  bulkEcho({ actor, type, length, copyTo }) {
    Assert.equal(length, really_long().length);
    this.conn
      .startBulkSend({
        actor,
        type,
        length,
      })
      .then(({ copyFrom }) => {
        // We'll just echo back the same thing
        const pipe = new Pipe(true, true, 0, 0, null);
        copyTo(pipe.outputStream).then(() => {
          pipe.outputStream.close();
        });
        copyFrom(pipe.inputStream).then(() => {
          pipe.inputStream.close();
        });
      });
  }

  bulkReply({ to, type }) {
    this.conn
      .startBulkSend({
        actor: to,
        type,
        length: really_long().length,
      })
      .then(({ copyFrom }) => {
        NetUtil.asyncFetch(
          {
            uri: NetUtil.newURI(getTestTempFile("bulk-input")),
            loadUsingSystemPrincipal: true,
          },
          input => {
            copyFrom(input).then(() => {
              input.close();
            });
          }
        );
      });
  }

  jsonReply({ length, copyTo }) {
    Assert.equal(length, really_long().length);

    const outputFile = getTestTempFile("bulk-output", true);
    outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

    const output = FileUtils.openSafeFileOutputStream(outputFile);

    return copyTo(output)
      .then(() => {
        FileUtils.closeSafeFileOutputStream(output);
        return verify_files();
      })
      .then(() => {
        return { allDone: true };
      }, do_throw);
  }
}

function add_test_bulk_actor() {
  ActorRegistry.addGlobalActor(
    {
      constructorName: "TestBulkActor",
      constructorFun: TestBulkActor,
    },
    "testBulk"
  );
}

/** * Reply Handlers ***/

var replyHandlers = {
  json(request) {
    // Receive JSON reply from server
    return new Promise(resolve => {
      request.on("json-reply", reply => {
        Assert.ok(reply.allDone);
        resolve();
      });
    });
  },

  bulk(request) {
    // Receive bulk data reply from server
    return new Promise(resolve => {
      request.on("bulk-reply", ({ length, copyTo }) => {
        Assert.equal(length, really_long().length);

        const outputFile = getTestTempFile("bulk-output", true);
        outputFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, parseInt("666", 8));

        const output = FileUtils.openSafeFileOutputStream(outputFile);

        copyTo(output).then(() => {
          FileUtils.closeSafeFileOutputStream(output);
          resolve(verify_files());
        });
      });
    });
  },
};

/** * Tests ***/

var test_bulk_request_cs = async function(
  transportFactory,
  actorType,
  replyType
) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let clientResolve;
  const clientDeferred = new Promise(resolve => {
    clientResolve = resolve;
  });

  let serverResolve;
  const serverDeferred = new Promise(resolve => {
    serverResolve = resolve;
  });

  let bulkCopyResolve;
  const bulkCopyDeferred = new Promise(resolve => {
    bulkCopyResolve = resolve;
  });

  const transport = await transportFactory();

  const client = new DevToolsClient(transport);
  client.connect().then(() => {
    client.mainRoot.rootForm.then(clientResolve);
  });

  function bulkSendReadyCallback({ copyFrom }) {
    NetUtil.asyncFetch(
      {
        uri: NetUtil.newURI(getTestTempFile("bulk-input")),
        loadUsingSystemPrincipal: true,
      },
      input => {
        copyFrom(input).then(() => {
          input.close();
          bulkCopyResolve();
        });
      }
    );
  }

  clientDeferred
    .then(response => {
      const request = client.startBulkRequest({
        actor: response.testBulk,
        type: actorType,
        length: really_long().length,
      });

      // Send bulk data to server
      request.on("bulk-send-ready", bulkSendReadyCallback);

      // Set up reply handling for this type
      replyHandlers[replyType](request).then(() => {
        client.close();
        transport.close();
      });
    })
    .catch(do_throw);

  DevToolsServer.on("connectionchange", type => {
    if (type === "closed") {
      serverResolve();
    }
  });

  return Promise.all([clientDeferred, bulkCopyDeferred, serverDeferred]);
};

var test_json_request_cs = async function(
  transportFactory,
  actorType,
  replyType
) {
  // Ensure test files are not present from a failed run
  cleanup_files();
  writeTestTempFile("bulk-input", really_long());

  let clientResolve;
  const clientDeferred = new Promise(resolve => {
    clientResolve = resolve;
  });

  let serverResolve;
  const serverDeferred = new Promise(resolve => {
    serverResolve = resolve;
  });

  const transport = await transportFactory();

  const client = new DevToolsClient(transport);
  await client.connect();
  client.mainRoot.rootForm.then(clientResolve);

  clientDeferred
    .then(response => {
      const request = client.request({
        to: response.testBulk,
        type: actorType,
      });

      // Set up reply handling for this type
      replyHandlers[replyType](request).then(() => {
        client.close();
        transport.close();
      });
    })
    .catch(do_throw);

  DevToolsServer.on("connectionchange", type => {
    if (type === "closed") {
      serverResolve();
    }
  });

  return Promise.all([clientDeferred, serverDeferred]);
};

/** * Test Utils ***/

function verify_files() {
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
