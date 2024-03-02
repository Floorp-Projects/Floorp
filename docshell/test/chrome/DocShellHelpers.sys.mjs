/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

export class DocShellHelpersParent extends JSWindowActorParent {
  static eventListener;

  // These static variables should be set when registering the actor
  // (currently doPageNavigation in docshell_helpers.js).
  static eventsToListenFor;
  static observers;

  constructor() {
    super();
  }
  receiveMessage({ name, data }) {
    if (name == "docshell_helpers:event") {
      let { event, originalTargetIsHTMLDocument } = data;

      if (this.constructor.eventsToListenFor.includes(event.type)) {
        this.constructor.eventListener(event, originalTargetIsHTMLDocument);
      }
    } else if (name == "docshell_helpers:observe") {
      let { topic } = data;

      this.constructor.observers.get(topic).call();
    }
  }
}

export class DocShellHelpersChild extends JSWindowActorChild {
  constructor() {
    super();
  }
  receiveMessage({ name }) {
    if (name == "docshell_helpers:preventBFCache") {
      // Add an RTCPeerConnection to prevent the page from being bfcached.
      let win = this.contentWindow;
      win.blockBFCache = new win.RTCPeerConnection();
    }
  }
  handleEvent(event) {
    if (
      Document.isInstance(event.originalTarget) &&
      event.originalTarget.isInitialDocument
    ) {
      dump(`TEST: ignoring a ${event.type} event for an initial about:blank\n`);
      return;
    }

    this.sendAsyncMessage("docshell_helpers:event", {
      event: {
        type: event.type,
        persisted: event.persisted,
        originalTarget: {
          title: event.originalTarget.title,
          location: event.originalTarget.location.href,
          visibilityState: event.originalTarget.visibilityState,
          hidden: event.originalTarget.hidden,
        },
      },
      originalTargetIsHTMLDocument: HTMLDocument.isInstance(
        event.originalTarget
      ),
    });
  }
  observe(subject, topic) {
    if (Window.isInstance(subject) && subject.document.isInitialDocument) {
      dump(`TEST: ignoring a topic notification for an initial about:blank\n`);
      return;
    }

    this.sendAsyncMessage("docshell_helpers:observe", { topic });
  }
}
