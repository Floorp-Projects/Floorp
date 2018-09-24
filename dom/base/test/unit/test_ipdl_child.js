"use strict";

IPDLGlob.registerProtocol("PTest", "resource://test/PTest.ipdl");
IPDLGlob.registerProtocol("PSubTest", "resource://test/PSubTest.ipdl");

// Child process tests
async function run_test() {
  IPDLGlob.registerTopLevelClass(TestChild);

  // Note that we stop the test in the parent process test
}

class TestChild extends IPDLGlob.PTestChild {
  allocPSubTest() {
    return new SubTestChild();
  }

  async recvPSubTestConstructor(protocol) {
    test_ipdlChildSendSyncMessage(protocol);
    await test_ipdlChildSendAsyncMessage(protocol);
  }

  recvAsyncMessage(input) {
    return input + 1;
  }
}

class SubTestChild extends IPDLGlob.PSubTestChild {
  recvAsyncMessage(input) {
    return input + 1;
  }
}

function test_ipdlChildSendSyncMessage(protocol) {
  Assert.equal(protocol.sendSyncMessage(42), 43);
}

async function test_ipdlChildSendAsyncMessage(protocol) {
  await protocol.sendAsyncMessage(42).then(res => {
    Assert.equal(res, 43);
  });
}
