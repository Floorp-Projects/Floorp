SpecialPowers.setBoolPref("media.webspeech.synth.enabled", true);

var gSpeechRegistry = SpecialPowers.Cc["@mozilla.org/synth-voice-registry;1"]
  .getService(SpecialPowers.Ci.nsISynthVoiceRegistry);

var gAddedVoices = [];

var TestSpeechServiceNoAudio = {
  QueryInterface: function(iid) {
    return this;
  },

  getInterfaces: function(c) {},

  getHelperForLanguage: function() {}
};

function synthAddVoice(aServiceName, aName, aLang, aIsLocal) {
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
}

function synthSetDefault(aUri, aIsDefault) {
  gSpeechRegistry.setDefaultVoice(aUri, aIsDefault);
  var voices = speechSynthesis.getVoices();
  for (var i in voices) {
    if (voices[i].voiceURI == aUri)
      ok(voices[i]['default'], "Voice set to default");
  }
}

function synthCleanup() {
  var voicesBefore = speechSynthesis.getVoices().length;
  var toRemove = gAddedVoices.length;
  var removeArgs;
  while ((removeArgs = gAddedVoices.shift()))
    gSpeechRegistry.removeVoice.apply(gSpeechRegistry.removeVoice, removeArgs);

  var voicesAfter = speechSynthesis.getVoices().length;
  is(voicesAfter, voicesBefore - toRemove, "Successfully removed test voices");
}
