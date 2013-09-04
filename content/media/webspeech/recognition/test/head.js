"use strict";

const DEFAULT_AUDIO_SAMPLE_FILE = "hello.ogg";
const SPEECH_RECOGNITION_TEST_REQUEST_EVENT_TOPIC = "SpeechRecognitionTest:RequestEvent";
const SPEECH_RECOGNITION_TEST_END_TOPIC = "SpeechRecognitionTest:End";

var errorCodes = {
  NO_SPEECH : "no-speech",
  ABORTED : "aborted",
  AUDIO_CAPTURE : "audio-capture",
  NETWORK : "network",
  NOT_ALLOWED : "not-allowed",
  SERVICE_NOT_ALLOWED : "service-not-allowed",
  BAD_GRAMMAR : "bad-grammar",
  LANGUAGE_NOT_SUPPORTED : "language-not-supported"
};

var Services = SpecialPowers.Cu.import("resource://gre/modules/Services.jsm").Services;

function EventManager(sr) {
  var self = this;
  var nEventsExpected = 0;
  self.eventsReceived = [];

  var allEvents = [
    "audiostart",
    "soundstart",
    "speechstart",
    "speechend",
    "soundend",
    "audioend",
    "result",
    "nomatch",
    "error",
    "start",
    "end"
  ];

  var eventDependencies = {
    "speechend": "speechstart",
    "soundent": "soundstart",
    "audioend": "audiostart"
  };

  var isDone = false;

  // AUDIO_DATA events are asynchronous,
  // so we queue events requested while they are being
  // issued to make them seem synchronous
  var isSendingAudioData = false;
  var queuedEventRequests = [];

  // register default handlers
  for (var i = 0; i < allEvents.length; i++) {
    (function (eventName) {
      sr["on" + eventName] = function (evt) {
        var message = "unexpected event: " + eventName;
        if (eventName == "error") {
          message += " -- " + evt.message;
        }

        ok(false, message);
        if (self.doneFunc && !isDone) {
          isDone = true;
          self.doneFunc();
        }
      };
    })(allEvents[i]);
  }

  self.expect = function EventManager_expect(eventName, cb) {
    nEventsExpected++;

    sr["on" + eventName] = function(evt) {
      self.eventsReceived.push(eventName);
      ok(true, "received event " + eventName);

      var dep = eventDependencies[eventName];
      if (dep) {
        ok(self.eventsReceived.indexOf(dep) >= 0,
           eventName + " must come after " + dep);
      }

      cb && cb(evt, sr);
      if (self.doneFunc && !isDone &&
          nEventsExpected === self.eventsReceived.length) {
        isDone = true;
        self.doneFunc();
      }
    }
  }

  self.requestFSMEvent = function EventManager_requestFSMEvent(eventName) {
    if (isSendingAudioData) {
      info("Queuing event " + eventName + " until we're done sending audio data");
      queuedEventRequests.push(eventName);
      return;
    }

    var subject = null;

    if (eventName === "EVENT_AUDIO_DATA") {
      isSendingAudioData = true;
      var audioTag = document.createElement("audio");
      audioTag.src = self.audioSampleFile;

      subject = audioTag.mozCaptureStreamUntilEnded();
      audioTag.addEventListener("ended", function() {
        info("Sample stream ended, requesting queued events");
        isSendingAudioData = false;
        while (queuedEventRequests.length) {
          self.requestFSMEvent(queuedEventRequests.shift());
        }
      });

      audioTag.play();
    }

    info("requesting " + eventName);
    Services.obs.notifyObservers(subject,
                                 SPEECH_RECOGNITION_TEST_REQUEST_EVENT_TOPIC,
                                 eventName);
  }

  self.requestTestEnd = function EventManager_requestTestEnd() {
    Services.obs.notifyObservers(null, SPEECH_RECOGNITION_TEST_END_TOPIC, null);
  }
}

function buildResultCallback(transcript) {
  return (function(evt) {
    is(evt.results[0][0].transcript, transcript, "expect correct transcript");
  });
}

function buildErrorCallback(errcode) {
  return (function(err) {
    is(err.error, errcode, "expect correct error code");
  });
}

function performTest(options) {
  var prefs = options.prefs;

  prefs.unshift(
    ["media.webspeech.recognition.enable", true],
    ["media.webspeech.test.enable", true]
  );

  SpecialPowers.pushPrefEnv({set: prefs}, function() {
    var sr = new SpeechRecognition();
    var em = new EventManager(sr);

    for (var eventName in options.expectedEvents) {
      var cb = options.expectedEvents[eventName];
      em.expect(eventName, cb);
    }

    em.doneFunc = function() {
      em.requestTestEnd();
      if (options.doneFunc) {
        options.doneFunc();
      }
    }

    em.audioSampleFile = DEFAULT_AUDIO_SAMPLE_FILE;
    if (options.audioSampleFile) {
      em.audioSampleFile = options.audioSampleFile;
    }

    for (var i = 0; i < options.eventsToRequest.length; i++) {
      em.requestFSMEvent(options.eventsToRequest[i]);
    }
  });
}
