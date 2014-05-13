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
    var interval = setInterval(function periodic() {
      if (checkFunc()) {
        ok(true, type + ' is ' + successMessage);
        clearInterval(interval);
        interval = null;
        done();
      }
    }, 200);
    return function cancel() {
      if (interval) {
        ok(false, type + ' (' + successMessage + ')' +
           ' failed after waiting full duration');
        clearInterval(interval);
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
    return periodicCheck('audio', testAudio,
                         (constraintApplied ? '' : 'not ') + 'silent', done);
  }

  function mkElement(type) {
    var display = document.getElementById('display');
    var e = document.createElement(type);
    e.width = 32;
    e.height = 24;
    display.appendChild(e);
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
