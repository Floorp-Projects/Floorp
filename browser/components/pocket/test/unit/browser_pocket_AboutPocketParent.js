/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { AboutPocketParent } = ChromeUtils.import(
  "resource:///actors/AboutPocketParent.jsm"
);
const { pktApi } = ChromeUtils.import("chrome://pocket/content/pktApi.jsm");
let aboutPocketParent;

function test_runner(test) {
  let testTask = async () => {
    // Before each
    const sandbox = sinon.createSandbox();
    aboutPocketParent = new AboutPocketParent();

    const manager = {
      isClosed: false,
    };
    const browsingContext = {
      topChromeWindow: {
        pktUI: {
          onShowSignup: sandbox.spy(),
          onShowSaved: sandbox.spy(),
          closePanel: sandbox.spy(),
          onOpenTabWithUrl: sandbox.spy(),
          onOpenTabWithPocketUrl: sandbox.spy(),
          resizePanel: sandbox.spy(),
          getPanelFrame: () => ({ setAttribute: () => {} }),
        },
      },
      embedderElement: {
        contentDocument: {
          nodePrincipal: "nodePrincipal",
          csp: "csp",
        },
      },
    };

    sandbox.stub(aboutPocketParent, "manager").get(() => manager);
    sandbox
      .stub(aboutPocketParent, "browsingContext")
      .get(() => browsingContext);

    try {
      await test({ sandbox });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

test_runner(async function test_AboutPocketParent_sendResponseMessageToPanel({
  sandbox,
}) {
  const sendAsyncMessage = sandbox.stub(aboutPocketParent, "sendAsyncMessage");

  aboutPocketParent.sendResponseMessageToPanel("PKT_testMessage", "1", {
    foo: 1,
  });

  const { args } = sendAsyncMessage.firstCall;

  Assert.ok(
    sendAsyncMessage.calledOnce,
    "Should fire sendAsyncMessage once with sendResponseMessageToPanel"
  );
  Assert.deepEqual(
    args,
    ["PKT_testMessage_response_1", { foo: 1 }],
    "Should fire sendAsyncMessage with proper args from sendResponseMessageToPanel"
  );
});

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_show_signup({
    sandbox,
  }) {
    await aboutPocketParent.receiveMessage({
      name: "PKT_show_signup",
    });

    const {
      onShowSignup,
    } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;

    Assert.ok(
      onShowSignup.calledOnce,
      "Should fire onShowSignup once with PKT_show_signup"
    );
  }
);

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_show_saved({
    sandbox,
  }) {
    await aboutPocketParent.receiveMessage({
      name: "PKT_show_saved",
    });

    const {
      onShowSaved,
    } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;

    Assert.ok(
      onShowSaved.calledOnce,
      "Should fire onShowSaved once with PKT_show_saved"
    );
  }
);

test_runner(async function test_AboutPocketParent_receiveMessage_PKT_close({
  sandbox,
}) {
  await aboutPocketParent.receiveMessage({
    name: "PKT_close",
  });

  const {
    closePanel,
  } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;

  Assert.ok(
    closePanel.calledOnce,
    "Should fire closePanel once with PKT_close"
  );
});

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_openTabWithUrl({
    sandbox,
  }) {
    await aboutPocketParent.receiveMessage({
      name: "PKT_openTabWithUrl",
      data: {
        payload: { foo: 1 },
        panelId: 1,
      },
    });

    const {
      onOpenTabWithUrl,
    } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;
    const { args } = onOpenTabWithUrl.firstCall;

    Assert.ok(
      onOpenTabWithUrl.calledOnce,
      "Should fire onOpenTabWithUrl once with PKT_openTabWithUrl"
    );
    Assert.deepEqual(
      args,
      [1, { foo: 1 }, "nodePrincipal", "csp"],
      "Should fire onOpenTabWithUrl with proper args from PKT_openTabWithUrl"
    );
  }
);

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_openTabWithPocketUrl({
    sandbox,
  }) {
    await aboutPocketParent.receiveMessage({
      name: "PKT_openTabWithPocketUrl",
      data: {
        payload: { foo: 1 },
        panelId: 1,
      },
    });

    const {
      onOpenTabWithPocketUrl,
    } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;
    const { args } = onOpenTabWithPocketUrl.firstCall;

    Assert.ok(
      onOpenTabWithPocketUrl.calledOnce,
      "Should fire onOpenTabWithPocketUrl once with PKT_openTabWithPocketUrl"
    );
    Assert.deepEqual(
      args,
      [1, { foo: 1 }, "nodePrincipal", "csp"],
      "Should fire onOpenTabWithPocketUrl with proper args from PKT_openTabWithPocketUrl"
    );
  }
);

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_resizePanel({
    sandbox,
  }) {
    const sendResponseMessageToPanel = sandbox.stub(
      aboutPocketParent,
      "sendResponseMessageToPanel"
    );
    await aboutPocketParent.receiveMessage({
      name: "PKT_resizePanel",
      data: {
        payload: { foo: 1 },
        panelId: 1,
      },
    });

    const {
      resizePanel,
    } = aboutPocketParent.browsingContext.topChromeWindow.pktUI;
    const { args } = resizePanel.firstCall;

    Assert.ok(
      resizePanel.calledOnce,
      "Should fire resizePanel once with PKT_resizePanel"
    );
    Assert.deepEqual(
      args,
      [{ foo: 1 }],
      "Should fire resizePanel with proper args from PKT_resizePanel"
    );
    Assert.ok(
      sendResponseMessageToPanel.calledOnce,
      "Should fire sendResponseMessageToPanel once with PKT_resizePanel"
    );
    Assert.deepEqual(
      sendResponseMessageToPanel.firstCall.args,
      ["PKT_resizePanel", 1],
      "Should fire sendResponseMessageToPanel with proper args from PKT_resizePanel"
    );
  }
);

test_runner(async function test_AboutPocketParent_receiveMessage_PKT_getTags({
  sandbox,
}) {
  const sendResponseMessageToPanel = sandbox.stub(
    aboutPocketParent,
    "sendResponseMessageToPanel"
  );
  await aboutPocketParent.receiveMessage({
    name: "PKT_getTags",
    data: {
      panelId: 1,
    },
  });
  Assert.ok(
    sendResponseMessageToPanel.calledOnce,
    "Should fire sendResponseMessageToPanel once with PKT_getTags"
  );
  Assert.deepEqual(
    sendResponseMessageToPanel.firstCall.args,
    ["PKT_getTags", 1, { tags: [], usedTags: [] }],
    "Should fire sendResponseMessageToPanel with proper args from PKT_getTags"
  );
});

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_getSuggestedTags({
    sandbox,
  }) {
    const sendResponseMessageToPanel = sandbox.stub(
      aboutPocketParent,
      "sendResponseMessageToPanel"
    );
    sandbox.stub(pktApi, "getSuggestedTagsForURL").callsFake((url, options) => {
      options.success({ suggested_tags: "foo" });
    });

    await aboutPocketParent.receiveMessage({
      name: "PKT_getSuggestedTags",
      data: {
        panelId: 1,
        payload: { url: "https://foo.com" },
      },
    });

    Assert.ok(
      pktApi.getSuggestedTagsForURL.calledOnce,
      "Should fire getSuggestedTagsForURL once with PKT_getSuggestedTags"
    );
    Assert.equal(
      pktApi.getSuggestedTagsForURL.firstCall.args[0],
      "https://foo.com",
      "Should fire getSuggestedTagsForURL with proper url from PKT_getSuggestedTags"
    );
    Assert.ok(
      sendResponseMessageToPanel.calledOnce,
      "Should fire sendResponseMessageToPanel once with PKT_getSuggestedTags"
    );
    Assert.deepEqual(
      sendResponseMessageToPanel.firstCall.args,
      [
        "PKT_getSuggestedTags",
        1,
        {
          status: "success",
          value: { suggestedTags: "foo" },
        },
      ],
      "Should fire sendResponseMessageToPanel with proper args from PKT_getSuggestedTags"
    );
  }
);

test_runner(async function test_AboutPocketParent_receiveMessage_PKT_addTags({
  sandbox,
}) {
  const sendResponseMessageToPanel = sandbox.stub(
    aboutPocketParent,
    "sendResponseMessageToPanel"
  );
  sandbox.stub(pktApi, "addTagsToURL").callsFake((url, tags, options) => {
    options.success();
  });

  await aboutPocketParent.receiveMessage({
    name: "PKT_addTags",
    data: {
      panelId: 1,
      payload: { url: "https://foo.com", tags: "tags" },
    },
  });

  Assert.ok(
    pktApi.addTagsToURL.calledOnce,
    "Should fire addTagsToURL once with PKT_addTags"
  );
  Assert.equal(
    pktApi.addTagsToURL.firstCall.args[0],
    "https://foo.com",
    "Should fire addTagsToURL with proper url from PKT_addTags"
  );
  Assert.equal(
    pktApi.addTagsToURL.firstCall.args[1],
    "tags",
    "Should fire addTagsToURL with proper tags from PKT_addTags"
  );
  Assert.ok(
    sendResponseMessageToPanel.calledOnce,
    "Should fire sendResponseMessageToPanel once with PKT_addTags"
  );
  Assert.deepEqual(
    sendResponseMessageToPanel.firstCall.args,
    [
      "PKT_addTags",
      1,
      {
        status: "success",
      },
    ],
    "Should fire sendResponseMessageToPanel with proper args from PKT_addTags"
  );
});

test_runner(
  async function test_AboutPocketParent_receiveMessage_PKT_deleteItem({
    sandbox,
  }) {
    const sendResponseMessageToPanel = sandbox.stub(
      aboutPocketParent,
      "sendResponseMessageToPanel"
    );
    sandbox.stub(pktApi, "deleteItem").callsFake((itemId, options) => {
      options.success();
    });

    await aboutPocketParent.receiveMessage({
      name: "PKT_deleteItem",
      data: {
        panelId: 1,
        payload: { itemId: "itemId" },
      },
    });

    Assert.ok(
      pktApi.deleteItem.calledOnce,
      "Should fire deleteItem once with PKT_deleteItem"
    );
    Assert.equal(
      pktApi.deleteItem.firstCall.args[0],
      "itemId",
      "Should fire deleteItem with proper itemId from PKT_deleteItem"
    );
    Assert.ok(
      sendResponseMessageToPanel.calledOnce,
      "Should fire sendResponseMessageToPanel once with PKT_deleteItem"
    );
    Assert.deepEqual(
      sendResponseMessageToPanel.firstCall.args,
      [
        "PKT_deleteItem",
        1,
        {
          status: "success",
        },
      ],
      "Should fire sendResponseMessageToPanel with proper args from PKT_deleteItem"
    );
  }
);
