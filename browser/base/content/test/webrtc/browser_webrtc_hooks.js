/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource:///modules/webrtcUI.jsm");

const ORIGIN = "https://example.com";

async function tryPeerConnection(browser, expectedError = null) {
  let errtype = await ContentTask.spawn(browser, null, async function() {
    let pc = new content.RTCPeerConnection();
    try {
      await pc.createOffer({offerToReceiveAudio: true});
      return null;
    } catch (err) {
      return err.name;
    }
  });

  let detail = expectedError ? `createOffer() threw a ${expectedError}`
      : "createOffer() succeeded";
  is(errtype, expectedError, detail);
}

// Helper for tests that use the peer-request-allowed and -blocked events.
// A test that expects some of those events does the following:
//  - call Events.on() before the test to setup event handlers
//  - call Events.expect(name) after a specific event is expected to have
//    occured.  This will fail if the event didn't occur, and will return
//    the details passed to the handler for furhter checking.
//  - call Events.off() at the end of the test to clean up.  At this point, if
//    any events were triggered that the test did not expect, the test fails.
const Events = {
  events: ["peer-request-allowed", "peer-request-blocked"],
  details: new Map(),
  handlers: new Map(),
  on() {
    for (let event of this.events) {
      let handler = data => {
        if (this.details.has(event)) {
          ok(false, `Got multiple ${event} events`);
        }
        this.details.set(event, data);
      };
      webrtcUI.on(event, handler);
      this.handlers.set(event, handler);
    }
  },
  expect(event) {
    let result = this.details.get(event);
    isnot(result, undefined, `${event} event was triggered`);
    this.details.delete(event);

    // All events should have a good origin
    is(result.origin, ORIGIN, `${event} event has correct origin`);

    return result;
  },
  off() {
    for (let event of this.events) {
      webrtcUI.off(event, this.handlers.get(event));
      this.handlers.delete(event);
    }
    for (let [event, ] of this.details) {
      ok(false, `Got unexpected event ${event}`);
    }
  },
};

var gTests = [
  {
    desc: "Basic peer-request-allowed event",
    run: async function testPeerRequestEvent(browser) {
      Events.on();

      await tryPeerConnection(browser);

      let details = Events.expect("peer-request-allowed");
      isnot(details.callID, undefined, "peer-request-allowed event includes callID");
      isnot(details.windowID, undefined, "peer-request-allowed event includes windowID");

      Events.off();
    },
  },

  {
    desc: "Immediate peer connection blocker can allow",
    run: async function testBlocker(browser) {
      Events.on();

      let blockerCalled = false;
      let blocker = params => {
        is(params.origin, ORIGIN, "Peer connection blocker origin parameter is correct");
        blockerCalled = true;
        return "allow";
      };

      webrtcUI.addPeerConnectionBlocker(blocker);

      await tryPeerConnection(browser);
      is(blockerCalled, true, "Blocker was called");
      Events.expect("peer-request-allowed");

      webrtcUI.removePeerConnectionBlocker(blocker);
      Events.off();
    },
  },

  {
    desc: "Deferred peer connection blocker can allow",
    run: async function testDeferredBlocker(browser) {
      Events.on();

      let blocker = params => Promise.resolve("allow");
      webrtcUI.addPeerConnectionBlocker(blocker);

      await tryPeerConnection(browser);
      Events.expect("peer-request-allowed");

      webrtcUI.removePeerConnectionBlocker(blocker);
      Events.off();
    },
  },

  {
    desc: "Immediate peer connection blocker can deny",
    run: async function testBlockerDeny(browser) {
      Events.on();

      let blocker = params => "deny";
      webrtcUI.addPeerConnectionBlocker(blocker);

      await tryPeerConnection(browser, "NotAllowedError");

      Events.expect("peer-request-blocked");

      webrtcUI.removePeerConnectionBlocker(blocker);
      Events.off();
    },
  },

  {
    desc: "Multiple blockers work (both allow)",
    run: async function testMultipleAllowBlockers(browser) {
      Events.on();

      let blocker1Called = false, blocker1 = params => {
        blocker1Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker1);

      let blocker2Called = false, blocker2 = params => {
        blocker2Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker2);

      await tryPeerConnection(browser);

      Events.expect("peer-request-allowed");
      ok(blocker1Called, "First blocker was called");
      ok(blocker2Called, "Second blocker was called");

      webrtcUI.removePeerConnectionBlocker(blocker1);
      webrtcUI.removePeerConnectionBlocker(blocker2);
      Events.off();
    },
  },

  {
    desc: "Multiple blockers work (allow then deny)",
    run: async function testAllowDenyBlockers(browser) {
      Events.on();

      let blocker1Called = false, blocker1 = params => {
        blocker1Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker1);

      let blocker2Called = false, blocker2 = params => {
        blocker2Called = true;
        return "deny";
      };
      webrtcUI.addPeerConnectionBlocker(blocker2);

      await tryPeerConnection(browser, "NotAllowedError");

      Events.expect("peer-request-blocked");
      ok(blocker1Called, "First blocker was called");
      ok(blocker2Called, "Second blocker was called");

      webrtcUI.removePeerConnectionBlocker(blocker1);
      webrtcUI.removePeerConnectionBlocker(blocker2);
      Events.off();
    },
  },

  {
    desc: "Multiple blockers work (deny first)",
    run: async function testDenyAllowBlockers(browser) {
      Events.on();

      let blocker1Called = false, blocker1 = params => {
        blocker1Called = true;
        return "deny";
      };
      webrtcUI.addPeerConnectionBlocker(blocker1);

      let blocker2Called = false, blocker2 = params => {
        blocker2Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker2);

      await tryPeerConnection(browser, "NotAllowedError");

      Events.expect("peer-request-blocked");
      ok(blocker1Called, "First blocker was called");
      ok(!blocker2Called, "Peer connection blocker after a deny is not invoked");

      webrtcUI.removePeerConnectionBlocker(blocker1);
      webrtcUI.removePeerConnectionBlocker(blocker2);
      Events.off();
    },
  },

  {
    desc: "Blockers may be removed",
    run: async function testRemoveBlocker(browser) {
      Events.on();

      let blocker1Called = false, blocker1 = params => {
        blocker1Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker1);

      let blocker2Called = false, blocker2 = params => {
        blocker2Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker2);
      webrtcUI.removePeerConnectionBlocker(blocker1);

      await tryPeerConnection(browser);

      Events.expect("peer-request-allowed");

      ok(!blocker1Called, "Removed peer connection blocker is not invoked");
      ok(blocker2Called, "Second peer connection blocker was invoked");

      webrtcUI.removePeerConnectionBlocker(blocker2);
      Events.off();
    },
  },

  {
    desc: "Blocker that throws is ignored",
    run: async function testBlockerThrows(browser) {
      Events.on();
      let blocker1Called = false, blocker1 = params => {
        blocker1Called = true;
        throw new Error("kaboom");
      };
      webrtcUI.addPeerConnectionBlocker(blocker1);

      let blocker2Called = false, blocker2 = params => {
        blocker2Called = true;
        return "allow";
      };
      webrtcUI.addPeerConnectionBlocker(blocker2);

      await tryPeerConnection(browser);

      Events.expect("peer-request-allowed");
      ok(blocker1Called, "First blocker was invoked");
      ok(blocker2Called, "Second blocker was invoked");

      webrtcUI.removePeerConnectionBlocker(blocker1);
      webrtcUI.removePeerConnectionBlocker(blocker2);
      Events.off();
    },
  },

  {
    desc: "Cancel peer request",
    run: async function testBlockerCancel(browser) {
      let blocker, blockerPromise = new Promise(resolve => {
        blocker = params => {
          resolve();
          // defer indefinitely
          return new Promise(innerResolve => {});
        };
      });
      webrtcUI.addPeerConnectionBlocker(blocker);

      await ContentTask.spawn(browser, null, async function() {
        (new content.RTCPeerConnection()).createOffer({offerToReceiveAudio: true});
      });

      await blockerPromise;

      let eventPromise = new Promise(resolve => {
        webrtcUI.on("peer-request-cancel", function listener(details) {
          resolve(details);
          webrtcUI.off("peer-request-cancel", listener);
        });
      });

      await ContentTask.spawn(browser, null, async function() {
        content.location.reload();
      });

      let details = await eventPromise;
      isnot(details.callID, undefined, "peer-request-cancel event includes callID");
      is(details.origin, ORIGIN, "peer-request-cancel event has correct origin");

      webrtcUI.removePeerConnectionBlocker(blocker);
    },
  },
];

add_task(async function test() {
  await runTests(gTests,
                 { cleanup() {
                     is(webrtcUI.peerConnectionBlockers.size, 0,
                        "Peer connection blockers list is empty");
                   }
                 });
});
