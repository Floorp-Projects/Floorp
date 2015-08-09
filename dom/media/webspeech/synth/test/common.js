function synthTestQueue(aTestArgs, aEndFunc) {
  var utterances = [];
  for (var i in aTestArgs) {
    var uargs = aTestArgs[i][0];
    var win = uargs.win || window;
    var u = new win.SpeechSynthesisUtterance(uargs.text);

    if (uargs.args) {
      for (var attr in uargs.args)
        u[attr] = uargs.args[attr];
    }

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
    win.speechSynthesis.speak(u);
  }

  ok(!speechSynthesis.speaking, "speechSynthesis is not speaking yet.");
  ok(speechSynthesis.pending, "speechSynthesis has an utterance queued.");
}

function loadFrame(frameId) {
  return new Promise(function(resolve, reject) {
    var frame = document.getElementById(frameId);
    frame.addEventListener('load', function (e) {
      frame.contentWindow.document.title = frameId;
      resolve(frame);
    });
    frame1.src = 'data:text/html,' + encodeURI('<html><head></head><body></body></html>');
  });
}

function testSynthState(win, expectedState) {
  for (var attr in expectedState) {
    is(win.speechSynthesis[attr], expectedState[attr],
      win.document.title + ": '" + attr + '" does not match');
  }
}