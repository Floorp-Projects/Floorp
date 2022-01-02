"use strict";
var EXPORTED_SYMBOLS = ["AllowJavascriptChild"];

class AllowJavascriptChild extends JSWindowActorChild {
  async receiveMessage(msg) {
    switch (msg.name) {
      case "CheckScriptsAllowed":
        return this.checkScriptsAllowed();
      case "CheckFiredLoadEvent":
        return this.contentWindow.wrappedJSObject.gFiredOnload;
      case "CreateIframe":
        return this.createIframe(msg.data.url);
    }
    return null;
  }

  handleEvent(event) {
    if (event.type === "load") {
      this.sendAsyncMessage("LoadFired");
    }
  }

  checkScriptsAllowed() {
    let win = this.contentWindow;

    win.wrappedJSObject.gFiredOnclick = false;
    win.document.body.click();
    return win.wrappedJSObject.gFiredOnclick;
  }

  async createIframe(url) {
    let doc = this.contentWindow.document;

    let iframe = doc.createElement("iframe");
    iframe.src = url;
    doc.body.appendChild(iframe);

    await new Promise(resolve => {
      iframe.addEventListener("load", resolve, { once: true });
    });

    return iframe.browsingContext;
  }
}
