var gSpeechRegistry = SpecialPowers.Cc["@mozilla.org/synth-voice-registry;1"]
  .getService(SpecialPowers.Ci.nsISynthVoiceRegistry);

var gAddedVoices = [];

function SpeechTaskCallback(onpause, onresume, oncancel) {
  this.onpause = onpause;
  this.onresume = onresume;
  this.oncancel = oncancel;
}

SpeechTaskCallback.prototype = {
  QueryInterface: function(iid) {
    return this;
  },

  getInterfaces: function(c) {},

  getScriptableHelper: function() {},

  onPause: function onPause() {
    if (this.onpause)
      this.onpause();
  },

  onResume: function onResume() {
    if (this.onresume)
      this.onresume();
  },

  onCancel: function onCancel() {
    if (this.oncancel)
      this.oncancel();
  }
};

var TestSpeechServiceWithAudio = SpecialPowers.wrapCallbackObject({
  CHANNELS: 1,
  SAMPLE_RATE: 16000,

  serviceType: SpecialPowers.Ci.nsISpeechService.SERVICETYPE_DIRECT_AUDIO,

  speak: function speak(aText, aUri, aRate, aPitch, aTask) {
    var task = SpecialPowers.wrap(aTask);

    window.setTimeout(
      function () {
        task.setup(SpecialPowers.wrapCallbackObject(new SpeechTaskCallback()), this.CHANNELS, this.SAMPLE_RATE);
        // 0.025 seconds per character.
        task.sendAudio(new Int16Array((this.SAMPLE_RATE/40)*aText.length), []);
        task.sendAudio(new Int16Array(0), []);
      }.bind(this), 0);
  },

  QueryInterface: function(iid) {
    return this;
  },

  getInterfaces: function(c) {},

  getScriptableHelper: function() {}
});

var TestSpeechServiceNoAudio = SpecialPowers.wrapCallbackObject({
  serviceType: SpecialPowers.Ci.nsISpeechService.SERVICETYPE_INDIRECT_AUDIO,

  speak: function speak(aText, aUri, aRate, aPitch, aTask) {
    var pair = this.expectedSpeaks.shift();
    if (pair) {
      // XXX: These tests do not happen in OOP
      var utterance = pair[0];
      var expected = pair[1];

      is(aText, utterance.text, "Speak text matches utterance text");

      var args = {uri: aUri, rate: aRate, pitch: aPitch};

      for (var attr in args) {
        if (expected[attr] != undefined)
          is(args[attr], expected[attr], "expected service arg " + attr);
      }
    }

    // If the utterance contains the phrase 'callback events', we will dispatch
    // an appropriate event for each callback method.
    var no_events = (aText.indexOf('callback events') < 0);
    // If the utterance contains the phrase 'never end', we don't immediately
    // end the 'synthesis' of the utterance.
    var end_utterance = (aText.indexOf('never end') < 0);

    var task = SpecialPowers.wrap(aTask);
    task.setup(SpecialPowers.wrapCallbackObject(new SpeechTaskCallback(
      function() {
        if (!no_events) {
          task.dispatchPause(1, 1.23);
        }
      },
      function() {
        if (!no_events) {
          task.dispatchResume(1, 1.23);
        }
      },
      function() {
        if (!no_events) {
          task.dispatchEnd(1, 1.23);
        }
      })));
    setTimeout(function () {
                 task.dispatchStart();
                 if (end_utterance) {
                   setTimeout(function () {
                                task.dispatchEnd(
                                  aText.length / 2.0, aText.length);
                              }, 0);
                 }
               }, 0);
  },

  QueryInterface: function(iid) {
    return this;
  },

  getInterfaces: function(c) {},

  getScriptableHelper: function() {},

  expectedSpeaks: []
});

function synthAddVoice(aServiceName, aName, aLang, aIsLocal) {
  if (SpecialPowers.isMainProcess()) {
    var voicesBefore = speechSynthesis.getVoices().length;
    var uri = "urn:moz-tts:mylittleservice:" + encodeURI(aName + '?' + aLang);
    gSpeechRegistry.addVoice(window[aServiceName], uri, aName, aLang, aIsLocal);

    gAddedVoices.push([window[aServiceName], uri]);
    var voicesAfter = speechSynthesis.getVoices().length;

    is(voicesBefore + 1, voicesAfter, "Voice added");
    var voice = speechSynthesis.getVoices()[voicesAfter - 1];
    is(voice.voiceURI, uri, "voice URI matches");
    is(voice.name, aName, "voice name matches");
    is(voice.lang, aLang, "voice lang matches");
    is(voice.localService, aIsLocal, "voice localService matches");

    return uri;
  } else {
    // XXX: It would be nice to check here that the child gets the voice
    // added update, but alas, it is aynchronous.
    var mm = SpecialPowers.Cc["@mozilla.org/childprocessmessagemanager;1"]
      .getService(SpecialPowers.Ci.nsISyncMessageSender);

    return mm.sendSyncMessage(
      'test:SpeechSynthesis:ipcSynthAddVoice',
      [aServiceName, aName, aLang, aIsLocal])[0];
  }
}

function synthSetDefault(aUri, aIsDefault) {
  if (SpecialPowers.isMainProcess()) {
    gSpeechRegistry.setDefaultVoice(aUri, aIsDefault);
    var voices = speechSynthesis.getVoices();
    for (var i in voices) {
      if (voices[i].voiceURI == aUri)
        ok(voices[i]['default'], "Voice set to default");
    }
  } else {
    // XXX: It would be nice to check here that the child gets the voice
    // added update, but alas, it is aynchronous.
    var mm = SpecialPowers.Cc["@mozilla.org/childprocessmessagemanager;1"]
      .getService(SpecialPowers.Ci.nsISyncMessageSender);

    return mm.sendSyncMessage(
      'test:SpeechSynthesis:ipcSynthSetDefault', [aUri, aIsDefault])[0];
  }
}

function synthCleanup() {
  if (SpecialPowers.isMainProcess()) {
    var voicesBefore = speechSynthesis.getVoices().length;
    var toRemove = gAddedVoices.length;
    var removeArgs;
    while ((removeArgs = gAddedVoices.shift()))
      gSpeechRegistry.removeVoice.apply(gSpeechRegistry.removeVoice, removeArgs);

    var voicesAfter = speechSynthesis.getVoices().length;
    is(voicesAfter, voicesBefore - toRemove, "Successfully removed test voices");
  } else {
    // XXX: It would be nice to check here that the child gets the voice
    // removed update, but alas, it is aynchronous.
    var mm = SpecialPowers.Cc["@mozilla.org/childprocessmessagemanager;1"]
      .getService(SpecialPowers.Ci.nsISyncMessageSender);
    mm.sendSyncMessage('test:SpeechSynthesis:ipcSynthCleanup');
  }
}

function synthTestQueue(aTestArgs, aEndFunc) {
  var utterances = [];
  for (var i in aTestArgs) {
    var uargs = aTestArgs[i][0];
    var u = new SpeechSynthesisUtterance(uargs.text);

    delete uargs.text;

    for (var attr in uargs)
      u[attr] = uargs[attr];

    function onend_handler(e) {
      is(e.target, utterances.shift(), "Target matches utterances");
      ok(!speechSynthesis.speaking, "speechSynthesis is not speaking.");

      isnot(e.eventType, 'error', "Error in utterance");

      if (utterances.length) {
        ok(speechSynthesis.pending, "other utterances queued");
      } else {
        ok(!speechSynthesis.pending, "queue is empty, nothing pending.");
        if (aEndFunc)
          aEndFunc();
      }
    }

    u.addEventListener('end', onend_handler);
    u.addEventListener('error', onend_handler);

    u.addEventListener(
      'error', function onerror_handler(e) {
        ok(false, "Error in speech utterance '" + e.target.text + "'");
      });

    utterances.push(u);
    TestSpeechServiceNoAudio.expectedSpeaks.push([u, aTestArgs[i][1]]);
    speechSynthesis.speak(u);
  }

  ok(!speechSynthesis.speaking, "speechSynthesis is not speaking yet.");
  ok(speechSynthesis.pending, "speechSynthesis has an utterance queued.");
}
