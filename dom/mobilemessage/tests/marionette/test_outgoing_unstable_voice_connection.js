/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const RECEIVER = "15555215555";

// The Book of Mozilla, 15:1
const MESSAGES = [
  "The twins of Mammon quarrelled.",
  "Their warring plunged the world into a new darkness, and the beast abhorred the darkness.",
  "So it began to move swiftly, and grew more powerful, and went forth and multiplied.",
  "And the beasts brought fire and light to the darkness."
];

/**
 * Send messages and verify that corresponding "sending" events are received.
 *
 * @return an array of message IDs.
 */
function sendMessagesAndVerifySending() {
  return new Promise(function(resolve, reject) {
    try {
      let eventCount = 0;
      let now = Date.now();
      let messageIds = [];
      manager.addEventListener("sending", function onevent(aEvent) {
        log("onsending event received.");

        let message = aEvent.message;
        let expectedBody = MESSAGES[eventCount++];
        messageIds.push(message.id);
        is(message.delivery, "sending", "message.delivery");
        is(message.deliveryStatus, "pending", "message.deliveryStatus");
        is(message.body, expectedBody, "message.body: expected '" + expectedBody
          + "'' but got '" + message.body + "'");

        // timestamp is in seconds.
        ok(Math.floor(message.timestamp / 1000) >= Math.floor(now / 1000),
           "expected " + message.timestamp + " >= " + now);

        // resolve when all messages are appeared "sending"
        if (eventCount == MESSAGES.length) {
          manager.removeEventListener("sending", onevent);
          resolve(messageIds);
        }
      });

      // send messages
      for (let body of MESSAGES) {
        manager.send(RECEIVER, body);
      }
    } catch (err) {
      log("Error: " + err);
      reject(err);
    }
  });
}

/**
 * Turn on voice connection, delete specified messages, and verify all the
 * corresponding "sent" / "failed" events are received.
 *
 * @param aMessageIdsToDelete
 *        An array of message ids to delete.
 * @param aExpectedSentMessages
 *        Expected successfully sent messages, ordered.
 * @param aExpectedFailures
 *        Expected number of failures.
 */
function turnOnVoiceDeleteMessagesAndVerify(aMessageIdsToDelete,
  aExpectedSentMessages, aExpectedFailures) {
  let promises = [];

  // Wait for "sent" and "failed" events.
  promises.push(new Promise(function(resolve, reject) {
    try {
      let sentEventCount = 0;
      let failedEventCount = 0;

      let onSentHandler = function(aEvent) {
        log("onsent event received.");

        let message = aEvent.message;
        let expectedBody = aExpectedSentMessages[sentEventCount++];
        is(message.delivery, "sent", "message.delivery");
        is(message.receiver, RECEIVER, "message.receiver");
        is(message.body, expectedBody, "message.body: expected '" + expectedBody
          + "'' but got '" + message.body + "'");

        tryResolve();
      }

      let onFailedHandler = function(aEvent) {
        log("onfailed event received.");
        failedEventCount++;
        tryResolve();
      }

      let tryResolve = function() {
        log("sentEventCount=" + sentEventCount + "; failedEventCount=" + failedEventCount);
        if (sentEventCount === aExpectedSentMessages.length &&
          failedEventCount === aExpectedFailures) {
          manager.removeEventListener("sent", onSentHandler);
          manager.removeEventListener("failed", onFailedHandler);
          resolve();
        }
      }

      manager.addEventListener("sent", onSentHandler);
      manager.addEventListener("failed", onFailedHandler);
    } catch (err) {
      log("Error: " + err);
      reject(err);
    }
  }));

  // Delete messages with given ids.
  promises.push(deleteMessagesById(aMessageIdsToDelete));

  // wait for 3 seconds and turn on voice connection.
  promises.push(new Promise(function(resolve, reject) {
      setTimeout(() => resolve(), 3000);
    }).then(() => setEmulatorVoiceStateAndWait("on")));

  return Promise.all(promises);
}

/**
 * Send messages on the second SIM and verify success while voice connection
 * on first SIM is disabled.
 *
 * @return an array of message IDs.
 */
function sendMessagesOnSecondSIM() {
  return new Promise(function(resolve, reject) {
    try {
      let eventCount = 0;
      let now = Date.now();
      let messageIds = [];
      manager.addEventListener("sent", function onevent(aEvent) {
        log("onsent event received.");

        let message = aEvent.message;
        let expectedBody = MESSAGES[eventCount++];
        messageIds.push(message.id);
        is(message.delivery, "sent", "message.delivery");
        is(message.receiver, RECEIVER, "message.receiver");
        is(message.body, expectedBody, "message.body: expected '" + expectedBody
          + "'' but got '" + message.body + "'");

        // timestamp is in seconds.
        ok(Math.floor(message.timestamp / 1000) >= Math.floor(now / 1000),
           "expected " + message.timestamp + " >= " + now);

        // resolve when all messages are appeared "sent"
        if (eventCount == MESSAGES.length) {
          manager.removeEventListener("sent", onevent);
          resolve(messageIds);
        }
      });

      // send messages
      for (let body of MESSAGES) {
        manager.send(RECEIVER, body, { serviceId: 1 });
      }
    } catch (err) {
      log("Error: " + err);
      reject(err);
    }
  });
}

startTestCommon(function testCaseMain() {
  return pushPrefEnv({ set: [['dom.sms.requestStatusReport', true]] })
    .then(() => ensureMobileConnection())
    .then(() => setEmulatorVoiceStateAndWait("unregistered"))
    .then(() => runIfMultiSIM(() => sendMessagesOnSecondSIM()))
    .then(() => sendMessagesAndVerifySending())
    // Delete the first message and wait for result.
    .then((aMessageIds) => turnOnVoiceDeleteMessagesAndVerify([aMessageIds[0]],
      MESSAGES.slice().splice(1, MESSAGES.length - 1), 1));
});
