/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function addInitialDataChannel(chain) {
  chain.insertBefore('PC_LOCAL_CREATE_OFFER', [
    ['PC_LOCAL_CREATE_DATA_CHANNEL',
      function (test) {
        var channel = test.pcLocal.createDataChannel({});

        is(channel.binaryType, "blob", channel + " is of binary type 'blob'");
        is(channel.readyState, "connecting", channel + " is in state: 'connecting'");

        is(test.pcLocal.signalingState, STABLE,
           "Create datachannel does not change signaling state");

        test.next();
      }
    ]
  ]);
  chain.insertAfter('PC_REMOTE_CREATE_ANSWER', [
    [
      'PC_LOCAL_SETUP_DATA_CHANNEL_CALLBACK',
      function (test) {
        test.waitForInitialDataChannel(test.pcLocal, function () {
          ok(true, test.pcLocal + " dataChannels[0] switched to 'open'");
        },
        // At this point a timeout failure will be of no value
        null);
        test.next();
      }
    ],
    [
      'PC_REMOTE_SETUP_DATA_CHANNEL_CALLBACK',
      function (test) {
        test.waitForInitialDataChannel(test.pcRemote, function () {
          ok(true, test.pcRemote + " dataChannels[0] switched to 'open'");
        },
        // At this point a timeout failure will be of no value
        null);
        test.next();
      }
    ]
  ]);
  chain.insertBefore('PC_LOCAL_CHECK_MEDIA_TRACKS', [
    [
      'PC_LOCAL_VERIFY_DATA_CHANNEL_STATE',
      function (test) {
        test.waitForInitialDataChannel(test.pcLocal, function() {
          test.next();
        }, function() {
          ok(false, test.pcLocal + " initial dataChannels[0] failed to switch to 'open'");
          //TODO: use stopAndExit() once bug 1019323 has landed
          unexpectedEventAndFinish(this, 'timeout')
          // to prevent test framework timeouts
          test.next();
        });
      }
    ],
    [
      'PC_REMOTE_VERIFY_DATA_CHANNEL_STATE',
      function (test) {
        test.waitForInitialDataChannel(test.pcRemote, function() {
          test.next();
        }, function() {
          ok(false, test.pcRemote + " initial dataChannels[0] failed to switch to 'open'");
          //TODO: use stopAndExit() once bug 1019323 has landed
          unexpectedEventAndFinish(this, 'timeout');
          // to prevent test framework timeouts
          test.next();
        });
      }
    ]
  ]);
  chain.removeAfter('PC_REMOTE_CHECK_ICE_CONNECTIONS');
  chain.append([
    [
      'SEND_MESSAGE',
      function (test) {
        var message = "Lorem ipsum dolor sit amet";

        test.send(message, function (channel, data) {
          is(data, message, "Message correctly transmitted from pcLocal to pcRemote.");

          test.next();
        });
      }
    ],
    [
      'SEND_BLOB',
      function (test) {
        var contents = ["At vero eos et accusam et justo duo dolores et ea rebum."];
        var blob = new Blob(contents, { "type" : "text/plain" });

        test.send(blob, function (channel, data) {
          ok(data instanceof Blob, "Received data is of instance Blob");
          is(data.size, blob.size, "Received data has the correct size.");

          getBlobContent(data, function (recv_contents) {
            is(recv_contents, contents, "Received data has the correct content.");

            test.next();
          });
        });
      }
    ],
    [
      'CREATE_SECOND_DATA_CHANNEL',
      function (test) {
        test.createDataChannel({ }, function (sourceChannel, targetChannel) {
          is(sourceChannel.readyState, "open", sourceChannel + " is in state: 'open'");
          is(targetChannel.readyState, "open", targetChannel + " is in state: 'open'");

          is(targetChannel.binaryType, "blob", targetChannel + " is of binary type 'blob'");
          is(targetChannel.readyState, "open", targetChannel + " is in state: 'open'");

          test.next();
        });
      }
    ],
    [
      'SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL',
      function (test) {
        var channels = test.pcRemote.dataChannels;
        var message = "Lorem ipsum dolor sit amet";

        test.send(message, function (channel, data) {
          is(channels.indexOf(channel), channels.length - 1, "Last channel used");
          is(data, message, "Received message has the correct content.");

          test.next();
        });
      }
    ],
    [
      'SEND_MESSAGE_THROUGH_FIRST_CHANNEL',
      function (test) {
        var message = "Message through 1st channel";
        var options = {
          sourceChannel: test.pcLocal.dataChannels[0],
          targetChannel: test.pcRemote.dataChannels[0]
        };

        test.send(message, function (channel, data) {
          is(test.pcRemote.dataChannels.indexOf(channel), 0, "1st channel used");
          is(data, message, "Received message has the correct content.");

          test.next();
        }, options);
      }
    ],
    [
      'SEND_MESSAGE_BACK_THROUGH_FIRST_CHANNEL',
      function (test) {
        var message = "Return a message also through 1st channel";
        var options = {
          sourceChannel: test.pcRemote.dataChannels[0],
          targetChannel: test.pcLocal.dataChannels[0]
        };

        test.send(message, function (channel, data) {
          is(test.pcLocal.dataChannels.indexOf(channel), 0, "1st channel used");
          is(data, message, "Return message has the correct content.");

          test.next();
        }, options);
      }
    ],
    [
      'CREATE_NEGOTIATED_DATA_CHANNEL',
      function (test) {
        var options = {negotiated:true, id: 5, protocol:"foo/bar", ordered:false,
          maxRetransmits:500};
        test.createDataChannel(options, function (sourceChannel2, targetChannel2) {
          is(sourceChannel2.readyState, "open", sourceChannel2 + " is in state: 'open'");
          is(targetChannel2.readyState, "open", targetChannel2 + " is in state: 'open'");

          is(targetChannel2.binaryType, "blob", targetChannel2 + " is of binary type 'blob'");
          is(targetChannel2.readyState, "open", targetChannel2 + " is in state: 'open'");

          if (options.id != undefined) {
            is(sourceChannel2.id, options.id, sourceChannel2 + " id is:" + sourceChannel2.id);
          }
          else {
            options.id = sourceChannel2.id;
          }
          var reliable = !options.ordered ? false : (options.maxRetransmits || options.maxRetransmitTime);
          is(sourceChannel2.protocol, options.protocol, sourceChannel2 + " protocol is:" + sourceChannel2.protocol);
          is(sourceChannel2.reliable, reliable, sourceChannel2 + " reliable is:" + sourceChannel2.reliable);
  /*
    These aren't exposed by IDL yet
          is(sourceChannel2.ordered, options.ordered, sourceChannel2 + " ordered is:" + sourceChannel2.ordered);
          is(sourceChannel2.maxRetransmits, options.maxRetransmits, sourceChannel2 + " maxRetransmits is:" +
       sourceChannel2.maxRetransmits);
          is(sourceChannel2.maxRetransmitTime, options.maxRetransmitTime, sourceChannel2 + " maxRetransmitTime is:" +
       sourceChannel2.maxRetransmitTime);
  */

          is(targetChannel2.id, options.id, targetChannel2 + " id is:" + targetChannel2.id);
          is(targetChannel2.protocol, options.protocol, targetChannel2 + " protocol is:" + targetChannel2.protocol);
          is(targetChannel2.reliable, reliable, targetChannel2 + " reliable is:" + targetChannel2.reliable);
  /*
    These aren't exposed by IDL yet
         is(targetChannel2.ordered, options.ordered, targetChannel2 + " ordered is:" + targetChannel2.ordered);
          is(targetChannel2.maxRetransmits, options.maxRetransmits, targetChannel2 + " maxRetransmits is:" +
       targetChannel2.maxRetransmits);
          is(targetChannel2.maxRetransmitTime, options.maxRetransmitTime, targetChannel2 + " maxRetransmitTime is:" +
       targetChannel2.maxRetransmitTime);
  */

          test.next();
        });
      }
    ],
    [
      'SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL2',
      function (test) {
        var channels = test.pcRemote.dataChannels;
        var message = "Lorem ipsum dolor sit amet";

        test.send(message, function (channel, data) {
          is(channels.indexOf(channel), channels.length - 1, "Last channel used");
          is(data, message, "Received message has the correct content.");

          test.next();
        });
      }
    ]
  ]);
}
