/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let gCallback;

export class CodeMirrorTestParent extends JSWindowActorParent {
  static setCallback(callback) {
    gCallback = callback;
  }

  receiveMessage(message) {
    if (gCallback) {
      gCallback(message.name, message.data);
    }
  }
}

export class CodeMirrorTestChild extends JSWindowActorChild {
  handleEvent(event) {
    if (event.type == "DOMWindowCreated") {
      this.contentWindow.wrappedJSObject.mozilla_setStatus = (
        statusMsg,
        type,
        customMsg
      ) => {
        this.sendAsyncMessage("setStatus", {
          statusMsg,
          type,
          customMsg,
        });
      };

      this.check();
    }
  }

  check() {
    const doc = this.contentWindow.document;
    const out = doc.getElementById("status");
    if (!out || !out.classList.contains("done")) {
      this.contentWindow.setTimeout(() => this.check(), 100);
      return;
    }

    this.sendAsyncMessage("done", {
      failed: this.contentWindow.wrappedJSObject.failed,
    });
  }
}
