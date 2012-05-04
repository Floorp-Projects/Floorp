function check_ogg(v, enabled) {
  function check(type, expected) {
    is(v.canPlayType(type), enabled ? expected : "no", type);
  }

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

  // Verify Opus support
  var OpusEnabled = undefined;
  try {
    OpusEnabled = SpecialPowers.getBoolPref("media.opus.enabled");
  } catch (ex) {
    // SpecialPowers failed, perhaps because Opus isn't compiled in
    console.log("media.opus.enabled pref not found; skipping Opus validation");
  }
  if (OpusEnabled !== undefined) {
    SpecialPowers.setBoolPref("media.opus.enabled", true);
    check("audio/ogg; codecs=opus", "probably");
    SpecialPowers.setBoolPref("media.opus.enabled", false);
    check("audio/ogg; codecs=opus", "");
    SpecialPowers.setBoolPref("media.opus.enabled", OpusEnabled);
  }

  // Unsupported Ogg codecs
  check("video/ogg; codecs=xyz", "");
  check("video/ogg; codecs=xyz,vorbis", "");
  check("video/ogg; codecs=vorbis,xyz", "");
}
