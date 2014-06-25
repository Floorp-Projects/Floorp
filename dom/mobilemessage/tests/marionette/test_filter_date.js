/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const NUMBER_OF_MESSAGES = 10;
const REMOTE = "5552229797";

function simulateIncomingSms() {
  let promise = Promise.resolve();
  let messages = [];

  for (let i = 0; i < NUMBER_OF_MESSAGES; i++) {
    let text = "Incoming SMS number " + i;
    promise = promise.then(() => sendTextSmsToEmulatorAndWait(REMOTE, text))
      .then(function(aMessage) {
        messages.push(aMessage);
        return messages;
      });
  }

  return promise;
}

function test(aStartDate, aEndDate, aExpectedMessages) {
  let filter = new MozSmsFilter();
  if (aStartDate) {
    filter.startDate = aStartDate;
  }
  if (aEndDate) {
    filter.endDate = aEndDate;
  }

  return getMessages(filter, false)
    .then(function(aFoundMessages) {
      log("  Found " + aFoundMessages.length + " messages, expected " +
          aExpectedMessages.length);
      is(aFoundMessages.length, aExpectedMessages.length, "aFoundMessages.length");
    });
}

function reduceMessages(aMessages, aCompare) {
  return aMessages.reduce(function(results, message) {
    if (aCompare(message.timestamp)) {
      results.push(message);
    }
    return results;
  }, []);
}

startTestCommon(function testCaseMain() {
  let startTime, endTime;
  let allMessages;

  return simulateIncomingSms()
    .then((aMessages) => {
      allMessages = aMessages;
      startTime = aMessages[0].timestamp;
      endTime = aMessages[NUMBER_OF_MESSAGES - 1].timestamp;
      log("startTime: " + startTime + ", endTime: " + endTime);
    })

    // Should return all messages.
    //
    .then(() => log("Testing [startTime, )"))
    .then(() => test(new Date(startTime), null, allMessages))
    .then(() => log("Testing (, endTime]"))
    .then(() => test(null, new Date(endTime), allMessages))
    .then(() => log("Testing [startTime, endTime]"))
    .then(() => test(new Date(startTime), new Date(endTime), allMessages))

    // Should return only messages with timestamp <= startTime.
    //
    .then(() => log("Testing [, startTime)"))
    .then(() => test(null, new Date(startTime),
                     reduceMessages(allMessages,
                                    (function(a, b) {
                                      return b <= a;
                                    }).bind(null, startTime))))

    // Should return only messages with timestamp <= startTime + 1.
    //
    .then(() => log("Testing [, startTime + 1)"))
    .then(() => test(null, new Date(startTime + 1),
                     reduceMessages(allMessages,
                                    (function(a, b) {
                                      return b <= a;
                                    }).bind(null, startTime + 1))))

    // Should return only messages with timestamp >= endTime.
    //
    .then(() => log("Testing [endTime, )"))
    .then(() => test(new Date(endTime), null,
                     reduceMessages(allMessages,
                                    (function(a, b) {
                                      return b >= a;
                                    }).bind(null, endTime))))

    // Should return only messages with timestamp >= endTime - 1.
    //
    .then(() => log("Testing [endTime - 1, )"))
    .then(() => test(new Date(endTime - 1), null,
                     reduceMessages(allMessages,
                                    (function(a, b) {
                                      return b >= a;
                                    }).bind(null, endTime - 1))))

    // Should return none.
    //
    .then(() => log("Testing [endTime + 1, )"))
    .then(() => test(new Date(endTime + 1), null, []))
    .then(() => log("Testing [endTime + 1, endTime + 86400000]"))
    .then(() => test(new Date(endTime + 1), new Date(endTime + 86400000), []))
    .then(() => log("Testing (, startTime - 1]"))
    .then(() => test(null, new Date(startTime - 1), []))
    .then(() => log("Testing [startTime - 86400000, startTime - 1]"))
    .then(() => test(new Date(startTime - 86400000), new Date(startTime - 1), []));
});
