/**
 * Default list of commands to execute for a PeerConnection test.
 */
var commandsPeerConnection = [
  [
    'PC_LOCAL_GUM',
    function (test) {
      test.pcLocal.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GUM',
    function (test) {
      test.pcRemote.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, "stable",
         "Initial local signalingState is 'stable'");
      is(test.pcRemote.signalingState, "stable",
         "Initial remote signalingState is 'stable'");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.createOffer(test.pcLocal, function () {
        is(test.pcLocal.signalingState, "stable",
           "Local create offer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.pcLocal._last_offer, function () {
        is(test.pcLocal.signalingState, "have-local-offer",
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test.pcLocal._last_offer, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "Remote createAnswer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test.pcRemote._last_answer, function () {
        is(test.pcLocal.signalingState, "stable",
           "signalingState after local setRemoteDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.pcRemote._last_answer, function () {
        is(test.pcRemote.signalingState, "stable",
           "signalingState after remote setLocalDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcLocal.checkMediaStreams(test.pcRemote.constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcRemote.checkMediaStreams(test.pcLocal.constraints);
      test.next();
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcLocal.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcRemote.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ]
];


/**
 * Default list of commands to execute for a Datachannel test.
 */
var commandsDataChannel = [
  [
    'PC_LOCAL_GUM',
    function (test) {
      test.pcLocal.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_GUM',
    function (test) {
      test.pcRemote.getAllUserMedia(function () {
        test.next();
      });
    }
  ],
  [
    'PC_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, "stable",
         "Initial local signalingState is stable");
      is(test.pcRemote.signalingState, "stable",
         "Initial remote signalingState is stable");
      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_DATA_CHANNEL',
    function (test) {
      var channel = test.pcLocal.createDataChannel({});

      is(channel.binaryType, "blob", channel + " is of binary type 'blob'");
      is(channel.readyState, "connecting", channel + " is in state: 'connecting'");

      is(test.pcLocal.signalingState, "stable",
         "Create datachannel does not change signaling state");

      test.next();
    }
  ],
  [
    'PC_LOCAL_CREATE_OFFER',
    function (test) {
      test.pcLocal.createOffer(function (offer) {
        is(test.pcLocal.signalingState, "stable",
           "Local create offer does not change signaling state");
        ok(offer.sdp.contains("m=application"),
           "m=application is contained in the SDP");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcLocal, test.pcLocal._last_offer, function () {
        is(test.pcLocal.signalingState, "have-local-offer",
           "signalingState after local setLocalDescription is 'have-local-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test.pcLocal._last_offer, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "signalingState after remote setRemoteDescription is 'have-remote-offer'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CREATE_ANSWER',
    function (test) {
      test.createAnswer(test.pcRemote, function () {
        is(test.pcRemote.signalingState, "have-remote-offer",
           "Remote create offer does not change signaling state");
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test.pcRemote._last_answer, function () {
        is(test.pcLocal.signalingState, "stable",
           "signalingState after local setRemoteDescription is 'stable'");
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_SET_LOCAL_DESCRIPTION',
    function (test) {
      test.setLocalDescription(test.pcRemote, test.pcRemote._last_answer,
        function (sourceChannel, targetChannel) {
          is(sourceChannel.readyState, "open", test.pcLocal + " is in state: 'open'");
          is(targetChannel.readyState, "open", test.pcRemote + " is in state: 'open'");

          is(test.pcRemote.signalingState, "stable",
             "signalingState after remote setLocalDescription is 'stable'");
          test.next();
        }
      );
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcLocal.checkMediaStreams(test.pcRemote.constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcRemote.checkMediaStreams(test.pcLocal.constraints);
      test.next();
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcLocal.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_FLOW_PRESENT',
    function (test) {
      test.pcRemote.checkMediaFlowPresent(function () {
        test.next();
      });
    }
  ],
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
    'CLOSE_LAST_OPENED_DATA_CHANNEL',
    function (test) {
      var channels = test.pcRemote.dataChannels;

      test.closeDataChannel(channels.length - 1, function (channel) {
        is(channel.readyState, "closed", "Channel is in state: 'closed'");

        test.next();
      });
    }
  ]
];
