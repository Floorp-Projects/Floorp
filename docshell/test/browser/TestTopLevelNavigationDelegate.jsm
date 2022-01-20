/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * To avoid a proliferation of JSMs just for this test, we implement
 * both the TopLevelNavigationDelegateChild and
 * TopLevelNavigationDelegateParent JSWindowActors here.
 *
 * Note that the implementation must be called TopLevelNavigationDelegate
 * in order for the nsDocShell to detect it.
 */
var EXPORTED_SYMBOLS = [
  "TopLevelNavigationDelegateChild",
  "TopLevelNavigationDelegateParent",
];

class TopLevelNavigationDelegateParent extends JSWindowActorParent {
  actorCreated() {
    this.seenURIStrings = [];
  }

  receiveMessage(message) {
    if (message.name == "ShouldNavigate") {
      this.seenURIStrings.push(message.data.URIString);
    }
  }
}

class TopLevelNavigationDelegateChild extends JSWindowActorChild {
  shouldNavigate(
    docShell,
    URI,
    loadType,
    referrer,
    hasPostData,
    triggeringPrincipal,
    csp
  ) {
    if (URI.spec.includes("example.org")) {
      this.sendAsyncMessage("ShouldNavigate", { URIString: URI.spec });
      this.contentWindow.dispatchEvent(
        new this.contentWindow.Event("TopLevelNavigationDelegateEvent", {
          bubbles: true,
        })
      );
      return false;
    }

    return true;
  }
}

TopLevelNavigationDelegateChild.prototype.QueryInterface = ChromeUtils.generateQI(
  ["nsITopLevelNavigationDelegate"]
);
