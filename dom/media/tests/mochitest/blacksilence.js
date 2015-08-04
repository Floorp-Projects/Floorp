(function(global) {
  'use strict';

  // an invertible check on the condition.
  // if the constraint is applied, then the check is direct
  // if not applied, then the result should be reversed
  function check(constraintApplied, condition, message) {
    var good = constraintApplied ? condition : !condition;
    message = (constraintApplied ? 'with' : 'without') +
      ' constraint: should ' + (constraintApplied ? '' : 'not ') +
      message + ' = ' + (good ? 'OK' : 'waiting...');
    info(message);
    return good;
  }

  function mkElement(type) {
    // This makes an unattached element.
    // It's not rendered to save the cycles that costs on b2g emulator
    // and it gets dropped (and GC'd) when the test is done.
    var e = document.createElement(type);
    e.width = 32;
    e.height = 24;
    document.getElementById('display').appendChild(e);
    return e;
  }

  // Runs checkFunc until it reports success.
  // This is kludgy, but you have to wait for media to start flowing, and it
  // can't be any old media, it has to include real data, for which we have no
  // reliable signals to use as a trigger.
  function periodicCheck(checkFunc) {
    var resolve;
    var done = false;
    // This returns a function so that we create 10 closures in the loop, not
    // one; and so that the timers don't all start straight away
    var waitAndCheck = counter => () => {
      if (done) {
        return Promise.resolve();
      }
      return new Promise(r => setTimeout(r, 200 << counter))
        .then(() => {
          if (checkFunc()) {
            done = true;
            resolve();
          }
        });
    };

    var chain = Promise.resolve();
    for (var i = 0; i < 10; ++i) {
      chain = chain.then(waitAndCheck(i));
    }
    return new Promise(r => resolve = r);
  }

  function isSilence(audioData) {
    var silence = true;
    for (var i = 0; i < audioData.length; ++i) {
      if (audioData[i] !== 128) {
        silence = false;
      }
    }
    return silence;
  }

  function checkAudio(constraintApplied, stream) {
    var audio = mkElement('audio');
    audio.srcObject = stream;
    audio.play();

    var context = new AudioContext();
    var source = context.createMediaStreamSource(stream);
    var analyser = context.createAnalyser();
    source.connect(analyser);
    analyser.connect(context.destination);

    return periodicCheck(() => {
      var sampleCount = analyser.frequencyBinCount;
      info('got some audio samples: ' + sampleCount);
      var buffer = new Uint8Array(sampleCount);
      analyser.getByteTimeDomainData(buffer);

      var silent = check(constraintApplied, isSilence(buffer),
                         'be silence for audio');
      return sampleCount > 0 && silent;
    }).then(() => {
      source.disconnect();
      analyser.disconnect();
      audio.pause();
      ok(true, 'audio is ' + (constraintApplied ? '' : 'not ') + 'silent');
    });
  }

  function checkVideo(constraintApplied, stream) {
    var video = mkElement('video');
    video.srcObject = stream;
    video.play();

    return periodicCheck(() => {
      try {
        var canvas = mkElement('canvas');
        var ctx = canvas.getContext('2d');
        // Have to guard drawImage with the try as well, due to bug 879717. If
        // we get an error, this round fails, but that failure is usually just
        // transitory.
        ctx.drawImage(video, 0, 0);
        ctx.getImageData(0, 0, 1, 1);
        return check(constraintApplied, false, 'throw on getImageData for video');
      } catch (e) {
        return check(constraintApplied, e.name === 'SecurityError',
                     'get a security error: ' + e.name);
      }
    }).then(() => {
      video.pause();
      ok(true, 'video is ' + (constraintApplied ? '' : 'not ') + 'protected');
    });
  }

  global.audioIsSilence = checkAudio;
  global.videoIsBlack = checkVideo;
}(this));
