"use strict";

IPDLGlob.registerProtocol("PTest", "resource://test/PTest.ipdl");
IPDLGlob.registerProtocol("PSubTest", "resource://test/PSubTest.ipdl");

// Parent process test starting
function run_test() {
  do_load_child_test_harness();
  // Switch test termination to manual
  do_test_pending();

  do_get_idle();

  IPDLGlob.registerTopLevelClass(TestParent);

  // Load the child process test script
  sendCommand('load("head_ipdl.js"); load("test_ipdl_child.js");', function() {

    // Setup child protocol and run its tests
    sendCommand("run_test();", async function() {
      // Run parent tests
      var protocol = IPDLGlob.getTopLevelInstances()[0];

      try {
        await test_ipdlParentSendAsyncMessage(protocol);
        await test_ipdlParentSendPSubTestConstructor(protocol);
      } catch (errMsg) {
        do_test_finished();
        throw new Error(errMsg);
      }

      // We're done, bye.
      do_test_finished();
    });
  });
}

class TestParent extends IPDLGlob.PTestParent {
  recvSyncMessage(input) {
    return input + 1;
  }

  recvAsyncMessage(input) {
    return input + 1;
  }

  allocPSubTest() {
    return new SubTestParent();
  }
}

class SubTestParent extends IPDLGlob.PSubTestParent {
  recvSyncMessage(input) {
    return input + 1;
  }

  recvAsyncMessage(input) {
    return input + 1;
  }
}

async function test_ipdlParentSendAsyncMessage(protocol) {
  await protocol.sendAsyncMessage(42).then(res => {
    equal(res, 43);
  });
}

async function test_ipdlParentSendPSubTestConstructor(protocol) {
  await protocol.sendPSubTestConstructor().then(async subprotocol => {
    await subprotocol.sendAsyncMessage(42).then(res => {
      equal(res, 43);
    });

    await subprotocol.send__delete__();
  });
}
