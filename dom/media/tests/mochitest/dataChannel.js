/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Returns the contents of a blob as text
 *
 * @param {Blob} blob
          The blob to retrieve the contents from
 */
function getBlobContent(blob) {
  return new Promise(resolve => {
    var reader = new FileReader();
    // Listen for 'onloadend' which will always be called after a success or failure
    reader.onloadend = event => resolve(event.target.result);
    reader.readAsText(blob);
  });
}

var commandsCreateDataChannel = [
  function PC_REMOTE_EXPECT_DATA_CHANNEL(test) {
    test.pcRemote.expectDataChannel();
  },

  function PC_LOCAL_CREATE_DATA_CHANNEL(test) {
    var channel = test.pcLocal.createDataChannel({});
    is(channel.binaryType, "blob", channel + " is of binary type 'blob'");
    is(channel.readyState, "connecting", channel + " is in state: 'connecting'");

    is(test.pcLocal.signalingState, STABLE,
       "Create datachannel does not change signaling state");
    return test.pcLocal.observedNegotiationNeeded;
  }
];

var commandsWaitForDataChannel = [
  function PC_LOCAL_VERIFY_DATA_CHANNEL_STATE(test) {
    return test.pcLocal.dataChannels[0].opened;
  },

  function PC_REMOTE_VERIFY_DATA_CHANNEL_STATE(test) {
    return test.pcRemote.nextDataChannel.then(channel => channel.opened);
  },
];

var commandsCheckDataChannel = [
  function SEND_MESSAGE(test) {
    var message = "Lorem ipsum dolor sit amet";

    return test.send(message).then(result => {
      is(result.data, message, "Message correctly transmitted from pcLocal to pcRemote.");
    });
  },

  function SEND_BLOB(test) {
    var contents = "At vero eos et accusam et justo duo dolores et ea rebum.";
    var blob = new Blob([contents], { "type" : "text/plain" });

    return test.send(blob).then(result => {
      ok(result.data instanceof Blob, "Received data is of instance Blob");
      is(result.data.size, blob.size, "Received data has the correct size.");

      return getBlobContent(result.data);
    }).then(recv_contents =>
            is(recv_contents, contents, "Received data has the correct content."));
  },

  function CREATE_SECOND_DATA_CHANNEL(test) {
    return test.createDataChannel({ }).then(result => {
      var sourceChannel = result.local;
      var targetChannel = result.remote;
      is(sourceChannel.readyState, "open", sourceChannel + " is in state: 'open'");
      is(targetChannel.readyState, "open", targetChannel + " is in state: 'open'");

      is(targetChannel.binaryType, "blob", targetChannel + " is of binary type 'blob'");
    });
  },

  function SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL(test) {
    var channels = test.pcRemote.dataChannels;
    var message = "I am the Omega";

    return test.send(message).then(result => {
      is(channels.indexOf(result.channel), channels.length - 1, "Last channel used");
      is(result.data, message, "Received message has the correct content.");
    });
  },


  function SEND_MESSAGE_THROUGH_FIRST_CHANNEL(test) {
    var message = "Message through 1st channel";
    var options = {
      sourceChannel: test.pcLocal.dataChannels[0],
      targetChannel: test.pcRemote.dataChannels[0]
    };

    return test.send(message, options).then(result => {
      is(test.pcRemote.dataChannels.indexOf(result.channel), 0, "1st channel used");
      is(result.data, message, "Received message has the correct content.");
    });
  },


  function SEND_MESSAGE_BACK_THROUGH_FIRST_CHANNEL(test) {
    var message = "Return a message also through 1st channel";
    var options = {
      sourceChannel: test.pcRemote.dataChannels[0],
      targetChannel: test.pcLocal.dataChannels[0]
    };

    return test.send(message, options).then(result => {
      is(test.pcLocal.dataChannels.indexOf(result.channel), 0, "1st channel used");
      is(result.data, message, "Return message has the correct content.");
    });
  },

  function CREATE_NEGOTIATED_DATA_CHANNEL(test) {
    var options = {
      negotiated:true,
      id: 5,
      protocol: "foo/bar",
      ordered: false,
      maxRetransmits: 500
    };
    return test.createDataChannel(options).then(result => {
      var sourceChannel2 = result.local;
      var targetChannel2 = result.remote;
      is(sourceChannel2.readyState, "open", sourceChannel2 + " is in state: 'open'");
      is(targetChannel2.readyState, "open", targetChannel2 + " is in state: 'open'");

      is(targetChannel2.binaryType, "blob", targetChannel2 + " is of binary type 'blob'");

      is(sourceChannel2.id, options.id, sourceChannel2 + " id is:" + sourceChannel2.id);
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
    });
  },

  function SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL2(test) {
    var channels = test.pcRemote.dataChannels;
    var message = "I am the walrus; Goo goo g'joob";

    return test.send(message).then(result => {
      is(channels.indexOf(result.channel), channels.length - 1, "Last channel used");
      is(result.data, message, "Received message has the correct content.");
    });
  }
];

var commandsCheckLargeXfer = [
  function SEND_BIG_BUFFER(test) {
    var size = 512*1024; // SCTP internal buffer is 256K, so we'll have ~256K queued
    var buffer = new ArrayBuffer(size);
    // note: type received is always blob for binary data
    var options = {};
    options.bufferedAmountLowThreshold = 64*1024;
    return test.send(buffer, options).then(result => {
      ok(result.data instanceof Blob, "Received data is of instance Blob");
      is(result.data.size, size, "Received data has the correct size.");
    });
  },
];

function addInitialDataChannel(chain) {
  chain.insertBefore('PC_LOCAL_CREATE_OFFER', commandsCreateDataChannel);
  chain.insertBefore('PC_LOCAL_WAIT_FOR_MEDIA_FLOW', commandsWaitForDataChannel);
  chain.removeAfter('PC_REMOTE_CHECK_ICE_CONNECTIONS');
  chain.append(commandsCheckLargeXfer);
  chain.append(commandsCheckDataChannel);
}
