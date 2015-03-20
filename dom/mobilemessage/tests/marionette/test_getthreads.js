/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 90000;
MARIONETTE_HEAD_JS = 'head.js';

/**
 * Create messages to be tested.
 *
 * @param aMessages
 *        An array of
 *          { 'incoming': [false|true],
 *            'address': [Phone Number]
 *            'text': [Text Body] };
 *
 * @return A deferred promise.
 */
function createMessages(aMessages) {
  let promise = Promise.resolve();
  aMessages.forEach((aMessage) => {
    promise = promise.then((aMessage.incoming) ?
      () => sendTextSmsToEmulatorAndWait(aMessage.address, aMessage.text) :
      () => sendSmsWithSuccess(aMessage.address, aMessage.text));
  });

  return promise;
}

function checkThreads(aMessages, aNotMerged) {
  return getAllThreads().then((aThreads) => {
    let threadCount = aThreads.length;

    if (aNotMerged) {
      // Threads are retrieved in reversed order of 'lastTimestamp'.
      aThreads.reverse();
      is(threadCount, aMessages.length, "Number of Threads.");
      for (let i = 0; i < threadCount; i++) {
        let thread = aThreads[i];
        let message = aMessages[i];
        is(thread.unreadCount, message.incoming ? 1 : 0, "Unread Count.");
        is(thread.participants.length, 1, "Number of Participants.");
        is(thread.participants[0], message.address, "Participants.");
        is(thread.body, message.text, "Thread Body.");
      }

      return;
    }

    let lastBody = aMessages[aMessages.length - 1].text;
    let unreadCount = 0;
    let mergedThread = aThreads[0];
    aMessages.forEach((aMessage) => {
      if (aMessage.incoming) {
        unreadCount++;
      }
    });
    is(threadCount, 1, "Number of Threads.");
    is(mergedThread.unreadCount, unreadCount, "Unread Count.");
    is(mergedThread.participants.length, 1, "Number of Participants.");
    is(mergedThread.participants[0], aMessages[0].address, "Participants.");
    // Thread is updated according to the device 'timestamp' of the message record
    // instead of the one from SMSC, so 'mergedThread.body' is expected to be the
    // same to the body for the last saved SMS.
    // See https://hg.mozilla.org/mozilla-central/annotate/436686833af0/dom/mobilemessage/gonk/MobileMessageDB.jsm#l2247
    is(mergedThread.body, lastBody, "Thread Body.");
  });
}

function testGetThreads(aMessages, aNotMerged) {
  aNotMerged = !!aNotMerged;
  log("aMessages: " + JSON.stringify(aMessages));
  log("aNotMerged: " + aNotMerged);
  return createMessages(aMessages)
    .then(() => checkThreads(aMessages, aNotMerged))
    .then(() => deleteAllMessages());
}

startTestCommon(function testCaseMain() {
  // [Thread 1]
  // One message only, body = "thread 1";
  // All sent message, unreadCount = 0;
  // One participant only, participants = ["5555211001"].
  return testGetThreads([{ incoming: false, address: "5555211001", text: "thread 1" }])
  // [Thread 2]
  // Two messages, body = "thread 2-2";
  // All sent message, unreadCount = 0;
  // One participant with two aliased addresses, participants = ["5555211002"].
    .then(() => testGetThreads([{ incoming: false, address: "5555211002", text: "thread 2-1" },
                                { incoming: false, address: "+15555211002", text: "thread 2-2" }]))
  // [Thread 3]
  // Two messages, body = "thread 3-2";
  // All sent message, unreadCount = 0;
  // One participant with two aliased addresses, participants = ["+15555211003"].
    .then(() => testGetThreads([{ incoming: false, address: "+15555211003", text: "thread 3-1" },
                                { incoming: false, address: "5555211003", text: "thread 3-2" }]))
  // [Thread 4]
  // One message only, body = "thread 4";
  // All received message, unreadCount = 1;
  // One participant only, participants = ["5555211004"].
    .then(() => testGetThreads([{ incoming: true, address: "5555211004", text: "thread 4" }]))
  // [Thread 5]
  // All received messages, unreadCount = 2;
  // One participant with two aliased addresses, participants = ["5555211005"].
    .then(() => testGetThreads([{ incoming: true, address: "5555211005", text: "thread 5-1" },
                                { incoming: true, address: "+15555211005", text: "thread 5-2" },]))
  // [Thread 6]
  // All received messages, unreadCount = 2;
  // One participant with two aliased addresses, participants = ["+15555211006"].
    .then(() => testGetThreads([{ incoming: true, address: "+15555211006", text: "thread 6-1" },
                                { incoming: true, address: "5555211006", text: "thread 6-2" }]))
  // [Thread 7]
  // 2 sent and then 2 received messages, unreadCount = 2;
  // One participant with two aliased addresses, participants = ["5555211007"].
    .then(() => testGetThreads([{ incoming: false, address: "5555211007", text: "thread 7-1" },
                                { incoming: false, address: "+15555211007", text: "thread 7-2" },
                                { incoming: true, address: "5555211007", text: "thread 7-3" },
                                { incoming: true, address: "+15555211007", text: "thread 7-4" }]))
  // [Thread 8]
  // 2 received and then 2 sent messages, unreadCount = 2;
  // One participant with two aliased addresses, participants = ["5555211008"].
    .then(() => testGetThreads([{ incoming: true, address: "5555211008", text: "thread 8-1" },
                                { incoming: true, address: "+15555211008", text: "thread 8-2" },
                                { incoming: false, address: "5555211008", text: "thread 8-3" },
                                { incoming: false, address: "+15555211008", text: "thread 8-4" }]))
  // [Thread 9]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["+15555211009"].
    .then(() => testGetThreads([{ incoming: false, address: "+15555211009", text: "thread 9-1" },
                                { incoming: false, address: "01115555211009", text: "thread 9-2" },
                                { incoming: false, address: "5555211009", text: "thread 9-3" }]))
  // [Thread 10]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["+15555211010"].
    .then(() => testGetThreads([{ incoming: false, address: "+15555211010", text: "thread 10-1" },
                                { incoming: false, address: "5555211010", text: "thread 10-2" },
                                { incoming: false, address: "01115555211010", text: "thread 10-3" }]))
  // [Thread 11]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["01115555211011"].
    .then(() => testGetThreads([{ incoming: false, address: "01115555211011", text: "thread 11-1" },
                                { incoming: false, address: "5555211011", text: "thread 11-2" },
                                { incoming: false, address: "+15555211011", text: "thread 11-3" }]))
  // [Thread 12]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["01115555211012"].
    .then(() => testGetThreads([{ incoming: false, address: "01115555211012", text: "thread 12-1" },
                                { incoming: false, address: "+15555211012", text: "thread 12-2" },
                                { incoming: false, address: "5555211012", text: "thread 12-3" }]))
  // [Thread 13]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["5555211013"].
    .then(() => testGetThreads([{ incoming: false, address: "5555211013", text: "thread 13-1" },
                                { incoming: false, address: "+15555211013", text: "thread 13-2" },
                                { incoming: false, address: "01115555211013", text: "thread 13-3" }]))
  // [Thread 14]
  // Three sent message, unreadCount = 0;
  // One participant with three aliased addresses, participants = ["5555211014"].
    .then(() => testGetThreads([{ incoming: false, address: "5555211014", text: "thread 14-1" },
                                { incoming: false, address: "01115555211014", text: "thread 14-2" },
                                { incoming: false, address: "+15555211014", text: "thread 14-3" }]))
  // [Thread 15]
  // One sent message, unreadCount = 0;
  // One participant but might be merged to 555211015, participants = ["5555211015"].
  // [Thread 16]
  // One sent message, unreadCount = 0;
  // One participant but might be merged to 5555211015, participants = ["555211015"].
    .then(() => testGetThreads([{ incoming: false, address: "5555211015", text: "thread 15-1" },
                                { incoming: false, address: "555211015", text: "thread 16-1" }],
                                true))
  // [Thread 17]
  // Brazil number format: +55-aa-nnnnnnnn. (2-digit area code and 8-digit number)
  // Two sent messages, unreadCount = 0;
  // One participant with two aliased addresses, participants = ["+551155211017"].
    .then(() => testGetThreads([{ incoming: false, address: "+551155211017", text: "thread 17-1" },
                                { incoming: false, address: "1155211017", text: "thread 17-2" }]))
  // [Thread 18]
  // Brazil number format: +55-aa-nnnnnnnn. (2-digit area code and 8-digit number)
  // All sent messages, unreadCount = 0;
  // One participant with two aliased addresses, participants = ["1155211018"].
    .then(() => testGetThreads([{ incoming: false, address: "1155211018", text: "thread 18-1" },
                                { incoming: false, address: "+551155211018", text: "thread 18-2" }]));
});
