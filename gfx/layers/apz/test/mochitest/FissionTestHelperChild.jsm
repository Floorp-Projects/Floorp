var EXPORTED_SYMBOLS = ["FissionTestHelperChild"];

// This code runs in the content process that holds the window to which
// this actor is attached. There is one instance of this class for each
// "inner window" (i.e. one per content document, including iframes/nested
// iframes).
// There is a 1:1 relationship between instances of this class and
// FissionTestHelperParent instances, and the pair are entangled such
// that they can communicate with each other regardless of which process
// they live in.

class FissionTestHelperChild extends JSWindowActorChild {
  constructor() {
    super();
    this._msgCounter = 0;
    this._oopifResponsePromiseResolvers = [];
  }

  cw() {
    return this.contentWindow.wrappedJSObject;
  }

  initialize() {
    // This exports a bunch of things into the content window so that
    // the test can access them. Most things are scoped inside the
    // FissionTestHelper object on the window to avoid polluting the global
    // namespace.

    let cw = this.cw();
    Cu.exportFunction(
      (cond, msg) => this.sendAsyncMessage("ok", { cond, msg }),
      cw,
      { defineAs: "ok" }
    );
    Cu.exportFunction(
      (a, b, msg) => this.sendAsyncMessage("is", { a, b, msg }),
      cw,
      { defineAs: "is" }
    );

    let FissionTestHelper = Cu.createObjectIn(cw, {
      defineAs: "FissionTestHelper",
    });
    FissionTestHelper.startTestPromise = new cw.Promise(
      Cu.exportFunction(resolve => {
        this._startTestPromiseResolver = resolve;
      }, cw)
    );

    Cu.exportFunction(this.subtestDone.bind(this), FissionTestHelper, {
      defineAs: "subtestDone",
    });

    Cu.exportFunction(this.sendToOopif.bind(this), FissionTestHelper, {
      defineAs: "sendToOopif",
    });
    Cu.exportFunction(this.fireEventInEmbedder.bind(this), FissionTestHelper, {
      defineAs: "fireEventInEmbedder",
    });
  }

  // Called by the subtest to indicate completion to the top-level browser-chrome
  // mochitest.
  subtestDone() {
    let cw = this.cw();
    if (cw.ApzCleanup) {
      cw.ApzCleanup.execute();
    }
    this.sendAsyncMessage("Test:Complete", {});
  }

  // Called by the subtest to eval some code in the OOP iframe. This returns
  // a promise that resolves to the return value from the eval.
  sendToOopif(iframeElement, stringToEval) {
    let browsingContextId = iframeElement.browsingContext.id;
    let msgId = ++this._msgCounter;
    let cw = this.cw();
    let responsePromise = new cw.Promise(
      Cu.exportFunction(resolve => {
        this._oopifResponsePromiseResolvers[msgId] = resolve;
      }, cw)
    );
    this.sendAsyncMessage("EmbedderToOopif", {
      browsingContextId,
      msgId,
      stringToEval,
    });
    return responsePromise;
  }

  // Called by OOP iframes to dispatch an event in the embedder window. This
  // can be used by the OOP iframe to asynchronously notify the embedder of
  // things that happen. The embedder can use promiseOneEvent from
  // helper_fission_utils.js to listen for these events.
  fireEventInEmbedder(eventType, data) {
    this.sendAsyncMessage("OopifToEmbedder", { eventType, data });
  }

  handleEvent(evt) {
    switch (evt.type) {
      case "FissionTestHelper:Init":
        this.initialize();
        break;
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "Test:Start":
        this._startTestPromiseResolver();
        delete this._startTestPromiseResolver;
        break;
      case "FromEmbedder":
        let evalResult = this.contentWindow.eval(msg.data.stringToEval);
        this.sendAsyncMessage("OopifToEmbedder", {
          msgId: msg.data.msgId,
          evalResult,
        });
        break;
      case "FromOopif":
        if (typeof msg.data.msgId == "number") {
          if (!(msg.data.msgId in this._oopifResponsePromiseResolvers)) {
            dump(
              "Error: FromOopif got a message with unknown numeric msgId in " +
                this.contentWindow.location.href +
                "\n"
            );
          }
          this._oopifResponsePromiseResolvers[msg.data.msgId](
            msg.data.evalResult
          );
          delete this._oopifResponsePromiseResolvers[msg.data.msgId];
        } else if (typeof msg.data.eventType == "string") {
          let cw = this.cw();
          let event = new cw.Event(msg.data.eventType);
          event.data = Cu.cloneInto(msg.data.data, cw);
          this.contentWindow.dispatchEvent(event);
        } else {
          dump(
            "Warning: Unrecognized FromOopif message received in " +
              this.contentWindow.location.href +
              "\n"
          );
        }
        break;
    }
  }
}
