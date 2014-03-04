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
    'PC_LOCAL_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
      is(test.pcLocal.signalingState, "stable",
         "Initial local signalingState is 'stable'");
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_INITIAL_SIGNALINGSTATE',
    function (test) {
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
        if (!test.pcRemote) {
          send_message({"offer": test.pcLocal._last_offer,
                        "media_constraints": test.pcLocal.constraints});
        }
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
    'PC_REMOTE_GET_OFFER',
    function (test) {
      if (test.pcLocal) {
        test._local_offer = test.pcLocal._last_offer;
        test._local_constraints = test.pcLocal.constraints;
        test.next();
      } else {
        wait_for_message().then(function(message) {
          ok("offer" in message, "Got an offer message");
          test._local_offer = new mozRTCSessionDescription(message.offer);
          test._local_constraints = message.media_constraints;
          test.next();
        });
      }
    }
  ],
  [
    'PC_REMOTE_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcRemote, test._local_offer, function () {
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
        if (!test.pcLocal) {
          send_message({"answer": test.pcRemote._last_answer,
                        "media_constraints": test.pcRemote.constraints});
        }
        test.next();
      });
    }
  ],
  [
    'PC_LOCAL_GET_ANSWER',
    function (test) {
      if (test.pcRemote) {
        test._remote_answer = test.pcRemote._last_answer;
        test._remote_constraints = test.pcRemote.constraints;
        test.next();
      } else {
        wait_for_message().then(function(message) {
          ok("answer" in message, "Got an answer message");
          test._remote_answer = new mozRTCSessionDescription(message.answer);
          test._remote_constraints = message.media_constraints;
          test.next();
        });
      }
    }
  ],
  [
    'PC_LOCAL_SET_REMOTE_DESCRIPTION',
    function (test) {
      test.setRemoteDescription(test.pcLocal, test._remote_answer, function () {
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
    'PC_LOCAL_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcLocal;

      function onIceConnectedSuccess () {
        ok(true, "pc_local: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        ok(false, "pc_local: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState());
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        ok(true, "pc_local: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        ok(false, "pc_local: ICE is already in bad state: " + myPc.iceConnectionState());
        myTest.next();
      }
    }
  ],
  [
    'PC_REMOTE_WAIT_FOR_ICE_CONNECTED',
    function (test) {
      var myTest = test;
      var myPc = myTest.pcRemote;

      function onIceConnectedSuccess () {
        ok(true, "pc_remote: ICE switched to 'connected' state");
        myTest.next();
      };
      function onIceConnectedFailed () {
        ok(false, "pc_remote: ICE failed to switch to 'connected' state: " + myPc.iceConnectionState());
        myTest.next();
      };

      if (myPc.isIceConnected()) {
        ok(true, "pc_remote: ICE is in connected state");
        myTest.next();
      } else if (myPc.isIceConnectionPending()) {
        myPc.waitForIceConnected(onIceConnectedSuccess, onIceConnectedFailed);
      } else {
        ok(false, "pc_remote: ICE is already in bad state: " + myPc.iceConnectionState());
        myTest.next();
      }
    }
  ],
  [
    'PC_LOCAL_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcLocal.checkMediaStreams(test._remote_constraints);
      test.next();
    }
  ],
  [
    'PC_REMOTE_CHECK_MEDIA_STREAMS',
    function (test) {
      test.pcRemote.checkMediaStreams(test._local_constraints);
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
    'PC_LOCAL_CHECK_STATS',
    function (test) {
      test.pcLocal.getStats(null, function(stats) {
        test.pcLocal.checkStats(stats);
        test.next();
      });
    }
  ],
  [
    'PC_REMOTE_CHECK_STATS',
    function (test) {
      test.pcRemote.getStats(null, function(stats) {
        test.pcRemote.checkStats(stats);
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
	} else {
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
  ],
  [
    'CLOSE_LAST_OPENED_DATA_CHANNEL2',
    function (test) {
      var channels = test.pcRemote.dataChannels;

      test.closeDataChannel(channels.length - 1, function (channel) {
        is(channel.readyState, "closed", "Channel is in state: 'closed'");

        test.next();
      });
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
