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

    u.addEventListener('error',
      (function (expectedError) {
        return function onerror_handler(e) {
          ok(expectedError, "Error in speech utterance '" + e.target.text + "'");
        };
      })(aTestArgs[i][1] ? aTestArgs[i][1].err : false));

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
    frame.src = 'about:blank';
  });
}

function waitForVoices(win) {
  return new Promise(resolve => {
    function resolver() {
      if (win.speechSynthesis.getVoices().length) {
        win.speechSynthesis.removeEventListener('voiceschanged', resolver);
        resolve();
      }
    }

    win.speechSynthesis.addEventListener('voiceschanged', resolver);
    resolver();
  });
}

function loadSpeechTest(fileName, prefs, frameId="testFrame") {
  loadFrame(frameId).then(frame => {
    waitForVoices(frame.contentWindow).then(
      () => document.getElementById("testFrame").src = fileName);
  });
}

function testSynthState(win, expectedState) {
  for (var attr in expectedState) {
    is(win.speechSynthesis[attr], expectedState[attr],
      win.document.title + ": '" + attr + '" does not match');
  }
}
