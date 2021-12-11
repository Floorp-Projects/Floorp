var EXPORTED_SYMBOLS = ["FissionTestHelperParent"];

// This code always runs in the parent process. There is one instance of
// this class for each "inner window" (should be one per content document,
// including iframes/nested iframes).
// There is a 1:1 relationship between instances of this class and
// FissionTestHelperChild instances, and the pair are entangled such
// that they can communicate with each other regardless of which process
// they live in.

class FissionTestHelperParent extends JSWindowActorParent {
  constructor() {
    super();
    this._testCompletePromise = new Promise(resolve => {
      this._testCompletePromiseResolver = resolve;
    });
  }

  embedderWindow() {
    let embedder = this.manager.browsingContext.embedderWindowGlobal;
    // embedder is of type WindowGlobalParent, defined in WindowGlobalActors.webidl
    if (!embedder) {
      dump("ERROR: no embedder found in FissionTestHelperParent\n");
    }
    return embedder;
  }

  docURI() {
    return this.manager.documentURI.spec;
  }

  // Returns a promise that is resolved when this parent actor receives a
  // "Test:Complete" message from the child.
  getTestCompletePromise() {
    return this._testCompletePromise;
  }

  startTest() {
    this.sendAsyncMessage("Test:Start", {});
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "ok":
        FissionTestHelperParent.SimpleTest.ok(
          msg.data.cond,
          this.docURI() + " | " + msg.data.msg
        );
        break;

      case "is":
        FissionTestHelperParent.SimpleTest.is(
          msg.data.a,
          msg.data.b,
          this.docURI() + " | " + msg.data.msg
        );
        break;

      case "Test:Complete":
        this._testCompletePromiseResolver();
        break;

      case "EmbedderToOopif":
        // This relays messages from the embedder to an OOP-iframe. The browsing
        // context id in the message data identifies the OOP-iframe.
        let oopifBrowsingContext = BrowsingContext.get(
          msg.data.browsingContextId
        );
        if (oopifBrowsingContext == null) {
          FissionTestHelperParent.SimpleTest.ok(
            false,
            "EmbedderToOopif couldn't find oopif"
          );
          break;
        }
        let oopifActor = oopifBrowsingContext.currentWindowGlobal.getActor(
          "FissionTestHelper"
        );
        if (!oopifActor) {
          FissionTestHelperParent.SimpleTest.ok(
            false,
            "EmbedderToOopif couldn't find oopif actor"
          );
          break;
        }
        oopifActor.sendAsyncMessage("FromEmbedder", msg.data);
        break;

      case "OopifToEmbedder":
        // This relays messages from the OOP-iframe to the top-level content
        // window which is embedding it.
        let embedderActor = this.embedderWindow().getActor("FissionTestHelper");
        if (!embedderActor) {
          FissionTestHelperParent.SimpleTest.ok(
            false,
            "OopifToEmbedder couldn't find embedder"
          );
          break;
        }
        embedderActor.sendAsyncMessage("FromOopif", msg.data);
        break;
    }
  }
}
