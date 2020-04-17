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
      check('video/ogg; codecs="vorbis, theora"', "probably");
      check("video/ogg; codecs=theora", "probably");

      resolve();
    });
  }

  // Verify Opus support
  function verify_opus_support() {
    return new Promise(function(resolve, reject) {
      var OpusEnabled = SpecialPowers.getBoolPref(
        "media.opus.enabled",
        undefined
      );
      if (OpusEnabled != undefined) {
        resolve();
      } else {
        console.log(
          "media.opus.enabled pref not found; skipping Opus validation"
        );
        reject();
      }
    });
  }

  function opus_enable() {
    return SpecialPowers.pushPrefEnv({
      set: [["media.opus.enabled", true]],
    }).then(function() {
      check("audio/ogg; codecs=opus", "probably");
    });
  }

  function opus_disable() {
    return SpecialPowers.pushPrefEnv({
      set: [["media.opus.enabled", false]],
    }).then(function() {
      check("audio/ogg; codecs=opus", "");
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
    .finally(unspported_ogg);
}
