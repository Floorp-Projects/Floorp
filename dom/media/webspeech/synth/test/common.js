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

    u.addEventListener('start',
      (function (expectedUri) {
        return function (e) {
          if (expectedUri) {
            var chosenVoice = SpecialPowers.wrap(e).target.chosenVoiceURI;
            is(chosenVoice, expectedUri, "Incorrect URI is used");
          }
        };
      })(aTestArgs[i][1] ? aTestArgs[i][1].uri : null));

    u.addEventListener('end', onend_handler);
    u.addEventListener('error', onend_handler);

    u.addEventListener(
      'error', function onerror_handler(e) {
        ok(false, "Error in speech utterance '" + e.target.text + "'");
      });

    utterances.push(u);
    speechSynthesis.speak(u);
  }

  ok(!speechSynthesis.speaking, "speechSynthesis is not speaking yet.");
  ok(speechSynthesis.pending, "speechSynthesis has an utterance queued.");
}
