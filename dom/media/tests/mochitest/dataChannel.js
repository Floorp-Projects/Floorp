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

    is(
      test.pcLocal.signalingState,
      STABLE,
      "Create datachannel does not change signaling state"
    );
    return test.pcLocal.observedNegotiationNeeded;
  },
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

    info("Sending message:" + message);
    return test.send(message).then(result => {
      is(
        result.data,
        message,
        "Message correctly transmitted from pcLocal to pcRemote."
      );
    });
  },

  function SEND_BLOB(test) {
    var contents = "At vero eos et accusam et justo duo dolores et ea rebum.";
    var blob = new Blob([contents], { type: "text/plain" });

    info("Sending blob");
    return test
      .send(blob)
      .then(result => {
        ok(result.data instanceof Blob, "Received data is of instance Blob");
        is(result.data.size, blob.size, "Received data has the correct size.");

        return getBlobContent(result.data);
      })
      .then(recv_contents =>
        is(recv_contents, contents, "Received data has the correct content.")
      );
  },

  function CREATE_SECOND_DATA_CHANNEL(test) {
    return test.createDataChannel({}).then(result => {
      is(
        result.remote.binaryType,
        "blob",
        "remote data channel is of binary type 'blob'"
      );
    });
  },

  function SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL(test) {
    var channels = test.pcRemote.dataChannels;
    var message = "I am the Omega";

    info("Sending message:" + message);
    return test.send(message).then(result => {
      is(
        channels.indexOf(result.channel),
        channels.length - 1,
        "Last channel used"
      );
      is(result.data, message, "Received message has the correct content.");
    });
  },

  function SEND_MESSAGE_THROUGH_FIRST_CHANNEL(test) {
    var message = "Message through 1st channel";
    var options = {
      sourceChannel: test.pcLocal.dataChannels[0],
      targetChannel: test.pcRemote.dataChannels[0],
    };

    info("Sending message:" + message);
    return test.send(message, options).then(result => {
      is(
        test.pcRemote.dataChannels.indexOf(result.channel),
        0,
        "1st channel used"
      );
      is(result.data, message, "Received message has the correct content.");
    });
  },

  function SEND_MESSAGE_BACK_THROUGH_FIRST_CHANNEL(test) {
    var message = "Return a message also through 1st channel";
    var options = {
      sourceChannel: test.pcRemote.dataChannels[0],
      targetChannel: test.pcLocal.dataChannels[0],
    };

    info("Sending message:" + message);
    return test.send(message, options).then(result => {
      is(
        test.pcLocal.dataChannels.indexOf(result.channel),
        0,
        "1st channel used"
      );
      is(result.data, message, "Return message has the correct content.");
    });
  },

  function CREATE_NEGOTIATED_DATA_CHANNEL_MAX_RETRANSMITS(test) {
    var options = {
      negotiated: true,
      id: 5,
      protocol: "foo/bar",
      ordered: false,
      maxRetransmits: 500,
    };
    return test.createDataChannel(options).then(result => {
      is(
        result.local.binaryType,
        "blob",
        result.remote + " is of binary type 'blob'"
      );
      is(
        result.local.id,
        options.id,
        result.local + " id is:" + result.local.id
      );
      is(
        result.local.protocol,
        options.protocol,
        result.local + " protocol is:" + result.local.protocol
      );
      is(
        result.local.reliable,
        false,
        result.local + " reliable is:" + result.local.reliable
      );
      is(
        result.local.ordered,
        options.ordered,
        result.local + " ordered is:" + result.local.ordered
      );
      is(
        result.local.maxRetransmits,
        options.maxRetransmits,
        result.local + " maxRetransmits is:" + result.local.maxRetransmits
      );
      is(
        result.local.maxPacketLifeTime,
        null,
        result.local + " maxPacketLifeTime is:" + result.local.maxPacketLifeTime
      );

      is(
        result.remote.binaryType,
        "blob",
        result.remote + " is of binary type 'blob'"
      );
      is(
        result.remote.id,
        options.id,
        result.remote + " id is:" + result.remote.id
      );
      is(
        result.remote.protocol,
        options.protocol,
        result.remote + " protocol is:" + result.remote.protocol
      );
      is(
        result.remote.reliable,
        false,
        result.remote + " reliable is:" + result.remote.reliable
      );
      is(
        result.remote.ordered,
        options.ordered,
        result.remote + " ordered is:" + result.remote.ordered
      );
      is(
        result.remote.maxRetransmits,
        options.maxRetransmits,
        result.remote + " maxRetransmits is:" + result.remote.maxRetransmits
      );
      is(
        result.remote.maxPacketLifeTime,
        null,
        result.remote +
          " maxPacketLifeTime is:" +
          result.remote.maxPacketLifeTime
      );
    });
  },

  function SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL2(test) {
    var channels = test.pcRemote.dataChannels;
    var message = "I am the walrus; Goo goo g'joob";

    info("Sending message:" + message);
    return test.send(message).then(result => {
      is(
        channels.indexOf(result.channel),
        channels.length - 1,
        "Last channel used"
      );
      is(result.data, message, "Received message has the correct content.");
    });
  },

  function CREATE_NEGOTIATED_DATA_CHANNEL_MAX_PACKET_LIFE_TIME(test) {
    var options = {
      ordered: false,
      maxPacketLifeTime: 10,
    };
    return test.createDataChannel(options).then(result => {
      is(
        result.local.binaryType,
        "blob",
        result.local + " is of binary type 'blob'"
      );
      is(
        result.local.protocol,
        "",
        result.local + " protocol is:" + result.local.protocol
      );
      is(
        result.local.reliable,
        false,
        result.local + " reliable is:" + result.local.reliable
      );
      is(
        result.local.ordered,
        options.ordered,
        result.local + " ordered is:" + result.local.ordered
      );
      is(
        result.local.maxRetransmits,
        null,
        result.local + " maxRetransmits is:" + result.local.maxRetransmits
      );
      is(
        result.local.maxPacketLifeTime,
        options.maxPacketLifeTime,
        result.local + " maxPacketLifeTime is:" + result.local.maxPacketLifeTime
      );

      is(
        result.remote.binaryType,
        "blob",
        result.remote + " is of binary type 'blob'"
      );
      is(
        result.remote.protocol,
        "",
        result.remote + " protocol is:" + result.remote.protocol
      );
      is(
        result.remote.reliable,
        false,
        result.remote + " reliable is:" + result.remote.reliable
      );
      is(
        result.remote.ordered,
        options.ordered,
        result.remote + " ordered is:" + result.remote.ordered
      );
      is(
        result.remote.maxRetransmits,
        null,
        result.remote + " maxRetransmits is:" + result.remote.maxRetransmits
      );
      is(
        result.remote.maxPacketLifeTime,
        options.maxPacketLifeTime,
        result.remote +
          " maxPacketLifeTime is:" +
          result.remote.maxPacketLifeTime
      );
    });
  },

  function SEND_MESSAGE_THROUGH_LAST_OPENED_CHANNEL3(test) {
    var channels = test.pcRemote.dataChannels;
    var message = "Nice to see you working maxPacketLifeTime";

    info("Sending message:" + message);
    return test.send(message).then(result => {
      is(
        channels.indexOf(result.channel),
        channels.length - 1,
        "Last channel used"
      );
      is(result.data, message, "Received message has the correct content.");
    });
  },
];

var commandsCheckLargeXfer = [
  function SEND_BIG_BUFFER(test) {
    var size = 2 * 1024 * 1024; // SCTP internal buffer is now 1MB, so use 2MB to ensure the buffer gets full
    var buffer = new ArrayBuffer(size);
    // note: type received is always blob for binary data
    var options = {};
    options.bufferedAmountLowThreshold = 64 * 1024;
    info("Sending arraybuffer");
    return test.send(buffer, options).then(result => {
      ok(result.data instanceof Blob, "Received data is of instance Blob");
      is(result.data.size, size, "Received data has the correct size.");
    });
  },
];

function addInitialDataChannel(chain) {
  chain.insertBefore("PC_LOCAL_CREATE_OFFER", commandsCreateDataChannel);
  chain.insertBefore(
    "PC_LOCAL_WAIT_FOR_MEDIA_FLOW",
    commandsWaitForDataChannel
  );
  chain.removeAfter("PC_REMOTE_CHECK_ICE_CONNECTIONS");
  chain.append(commandsCheckDataChannel);
}
