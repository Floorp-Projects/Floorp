async function check_webm(v, enabled) {
  function check(type, expected) {
    is(
      v.canPlayType(type),
      enabled ? expected : "",
      type + "='" + expected + "'"
    );
  }

  // WebM types
  check("video/webm", "maybe");
  check("audio/webm", "maybe");

  var video = ["vp8", "vp8.0", "vp9", "vp9.0"];
  var audio = ["vorbis", "opus"];

  audio.forEach(function(acodec) {
    check("audio/webm; codecs=" + acodec, "probably");
    check("video/webm; codecs=" + acodec, "probably");
  });
  video.forEach(function(vcodec) {
    check("video/webm; codecs=" + vcodec, "probably");
    audio.forEach(function(acodec) {
      check('video/webm; codecs="' + vcodec + ", " + acodec + '"', "probably");
      check('video/webm; codecs="' + acodec + ", " + vcodec + '"', "probably");
    });
  });

  // Unsupported WebM codecs
  check("video/webm; codecs=xyz", "");
  check("video/webm; codecs=xyz,vorbis", "");
  check("video/webm; codecs=vorbis,xyz", "");

  await SpecialPowers.pushPrefEnv({ set: [["media.av1.enabled", true]] });
  check('video/webm; codecs="av1"', "probably");

  await SpecialPowers.pushPrefEnv({ set: [["media.av1.enabled", false]] });
  check('video/webm; codecs="av1"', "");
}
