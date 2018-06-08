/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  initTestDebuggerServer();
  add_test_bulk_actor();

  add_task(async function() {
    await test_string_error(socket_transport, json_reply);
    await test_string_error(local_transport, json_reply);
    DebuggerServer.destroy();
  });

  run_next_test();
}

/** * Sample Bulk Actor ***/

function TestBulkActor() {}

TestBulkActor.prototype = {

  actorPrefix: "testBulk",

  jsonReply: function({length, reader, reply, done}) {
    Assert.equal(length, really_long().length);

    return {
      allDone: true
    };
  }

};

TestBulkActor.prototype.requestTypes = {
  "jsonReply": TestBulkActor.prototype.jsonReply
};

function add_test_bulk_actor() {
  DebuggerServer.addGlobalActor({
    constructorName: "TestBulkActor",
    constructorFun: TestBulkActor,
  }, "testBulk");
}

/** * Tests ***/

var test_string_error = async function(transportFactory, onReady) {
  const transport = await transportFactory();

  const client = new DebuggerClient(transport);
  return client.connect().then(([app, traits]) => {
    Assert.equal(traits.bulk, true);
    return client.listTabs();
  }).then(response => {
    return onReady(client, response);
  }).then(() => {
    client.close();
    transport.close();
  });
};

/** * Reply Types ***/

function json_reply(client, response) {
  const reallyLong = really_long();

  const request = client.startBulkRequest({
    actor: response.testBulk,
    type: "jsonReply",
    length: reallyLong.length
  });

  // Send bulk data to server
  const copyDeferred = defer();
  request.on("bulk-send-ready", ({writer, done}) => {
    const input = Cc["@mozilla.org/io/string-input-stream;1"]
                  .createInstance(Ci.nsIStringInputStream);
    input.setData(reallyLong, reallyLong.length);
    try {
      writer.copyFrom(input, () => {
        input.close();
        done();
      });
      do_throw(new Error("Copying should fail, the stream is not async."));
    } catch (e) {
      Assert.ok(true);
      copyDeferred.resolve();
    }
  });

  return copyDeferred.promise;
}
