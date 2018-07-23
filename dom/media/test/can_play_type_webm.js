async function check_webm(v, enabled) {
  function check(type, expected) {
    is(v.canPlayType(type), enabled ? expected : "", type + "='" + expected + "'");
  }

  // WebM types
  check("video/webm", "maybe");
  check("audio/webm", "maybe");

  var video = ['vp8', 'vp8.0', 'vp9', 'vp9.0'];
  var audio = ['vorbis', 'opus'];

  audio.forEach(function(acodec) {
    check("audio/webm; codecs=" + acodec, "probably");
    check("video/webm; codecs=" + acodec, "probably");
  });
  video.forEach(function(vcodec) {
    check("video/webm; codecs=" + vcodec, "probably");
    audio.forEach(function(acodec) {
        check("video/webm; codecs=\"" + vcodec + ", " + acodec + "\"", "probably");
        check("video/webm; codecs=\"" + acodec + ", " + vcodec + "\"", "probably");
    });
  });

  // Unsupported WebM codecs
  check("video/webm; codecs=xyz", "");
  check("video/webm; codecs=xyz,vorbis", "");
  check("video/webm; codecs=vorbis,xyz", "");

  function getPref(name) {
    var pref = false;
    try {
      pref = SpecialPowers.getBoolPref(name);
    } catch(ex) { }
    return pref;
  }

  function isWindows32() {
    return navigator.userAgent.includes("Windows") &&
           !navigator.userAgent.includes("Win64");
  }

  function isAndroid() {
    return navigator.userAgent.includes("Android");
  }

  const haveAv1 = getPref("media.av1.enabled");
  check("video/webm; codecs=\"av1\"", haveAv1 ? "probably" : "");

  await SpecialPowers.pushPrefEnv({"set": [["media.av1.enabled", true]]});
  // AV1 is disabled on Windows 32 bits (bug 1475564) and Android (bug 1368843)
  check("video/webm; codecs=\"av1\"", (isWindows32() || isAndroid()) ? "" : "probably");

  await SpecialPowers.pushPrefEnv({"set": [["media.av1.enabled", false]]});
  check("video/webm; codecs=\"av1\"", "");
}
