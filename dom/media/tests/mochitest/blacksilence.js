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

  function isSilence(audioData) {
    var silence = true;
    for (var i = 0; i < audioData.length; ++i) {
      if (audioData[i] !== 128) {
        silence = false;
      }
    }
    return silence;
  }

  function periodicCheck(type, checkFunc, successMessage, done) {
    var num = 0;
    var timeout;
    function periodic() {
      if (checkFunc()) {
        ok(true, type + ' is ' + successMessage);
        done();
      } else {
        setupNext();
      }
    }
    function setupNext() {
      // exponential backoff on the timer
      // on a very slow system (like the b2g emulator) a long timeout is
      // necessary, but we want to run fast if we can
      timeout = setTimeout(periodic, 200 << num);
      num++;
    }

    setupNext();

    return function cancel() {
      if (timeout) {
        ok(false, type + ' (' + successMessage + ')' +
           ' failed after waiting full duration');
        clearTimeout(timeout);
        done();
      }
    };
  }

  function checkAudio(constraintApplied, stream, done) {
    var context = new AudioContext();
    var source = context.createMediaStreamSource(stream);
    var analyser = context.createAnalyser();
    source.connect(analyser);
    analyser.connect(context.destination);

    function testAudio() {
      var sampleCount = analyser.frequencyBinCount;
      info('got some audio samples: ' + sampleCount);
      var bucket = new ArrayBuffer(sampleCount);
      var view = new Uint8Array(bucket);
      analyser.getByteTimeDomainData(view);

      var silent = check(constraintApplied, isSilence(view), 'be silence for audio');
      return sampleCount > 0 && silent;
    }
    function disconnect() {
      source.disconnect();
      analyser.disconnect();
      done();
    }
    return periodicCheck('audio', testAudio,
                         (constraintApplied ? '' : 'not ') + 'silent', disconnect);
  }

  function mkElement(type) {
    // this makes an unattached element
    // it's not rendered to save the cycles that costs on b2g emulator
    // and it gets droped (and GC'd) when the test is done
    var e = document.createElement(type);
    e.width = 32;
    e.height = 24;
    return e;
  }

  function checkVideo(constraintApplied, stream, done) {
    var video = mkElement('video');
    video.mozSrcObject = stream;

    var ready = false;
    video.onplaying = function() {
      ready = true;
    }
    video.play();

    function tryToRenderToCanvas() {
      if (!ready) {
        info('waiting for video to start');
        return false;
      }

      try {
        // every try needs a new canvas, otherwise a taint from an earlier call
        // will affect subsequent calls
        var canvas = mkElement('canvas');
        var ctx = canvas.getContext('2d');
        // have to guard drawImage with the try as well, due to bug 879717
        // if we get an error, this round fails, but that failure is usually
        // just transitory
        ctx.drawImage(video, 0, 0);
        ctx.getImageData(0, 0, 1, 1);
        return check(constraintApplied, false, 'throw on getImageData for video');
      } catch (e) {
        return check(constraintApplied, e.name === 'SecurityError',
                     'get a security error: ' + e.name);
      }
    }

    return periodicCheck('video', tryToRenderToCanvas,
                         (constraintApplied ? '' : 'not ') + 'protected', done);
  }

  global.audioIsSilence = checkAudio;
  global.videoIsBlack = checkVideo;
}(this));
