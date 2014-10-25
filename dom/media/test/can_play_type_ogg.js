
function check_ogg(v, enabled, finish) {
  function check(type, expected) {
    is(v.canPlayType(type), enabled ? expected : "", type);
  }

  function basic_test() {
    return new Promise(function(resolve, reject) {
      // Ogg types
      check("video/ogg", "maybe");
      check("audio/ogg", "maybe");
      check("application/ogg", "maybe");

      // Supported Ogg codecs
      check("audio/ogg; codecs=vorbis", "probably");
      check("video/ogg; codecs=vorbis", "probably");
      check("video/ogg; codecs=vorbis,theora", "probably");
      check("video/ogg; codecs=\"vorbis, theora\"", "probably");
      check("video/ogg; codecs=theora", "probably");

      resolve();
    });
  }

  // Verify Opus support
  function verify_opus_support() {
    return new Promise(function(resolve, reject) {
      var OpusEnabled = undefined;
      try {
        OpusEnabled = SpecialPowers.getBoolPref("media.opus.enabled");
      } catch (ex) {
        // SpecialPowers failed, perhaps because Opus isn't compiled in
        console.log("media.opus.enabled pref not found; skipping Opus validation");
      }
      if (OpusEnabled != undefined) {
        resolve();
      } else {
        reject();
      }
    });
  }

  function opus_enable() {
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({"set": [['media.opus.enabled', true]]},
                                function() {
                                  check("audio/ogg; codecs=opus", "probably");
                                  resolve();
                                });
    });
  }

  function opus_disable() {
    return new Promise(function(resolve, reject) {
      SpecialPowers.pushPrefEnv({"set": [['media.opus.enabled', false]]},
                                function() {
                                  check("audio/ogg; codecs=opus", "");
                                  resolve();
                                });
    });
  }

  function unspported_ogg() {
    // Unsupported Ogg codecs
    check("video/ogg; codecs=xyz", "");
    check("video/ogg; codecs=xyz,vorbis", "");
    check("video/ogg; codecs=vorbis,xyz", "");

    finish.call();
  }

  basic_test()
  .then(verify_opus_support)
  .then(opus_enable)
  .then(opus_disable)
  .then(unspported_ogg, unspported_ogg);

}
