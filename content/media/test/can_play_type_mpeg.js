function check_mp4(v, enabled) {
  function check(type, expected) {
    var ex = enabled ? expected : "";
    is(v.canPlayType(type), ex, type + "='" + ex + "'");
  }

  check("video/mp4", "maybe");
  check("audio/mp4", "maybe");
  check("audio/x-m4a", "maybe");

  // Not the MIME type that other browsers respond to, so we won't either.
  check("audio/m4a", "");
  // Only Safari responds affirmatively to "audio/aac",
  // so we'll let x-m4a cover aac support.
  check("audio/aac", "");

  check("video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\"", "probably");
  check("video/mp4; codecs=\"avc1.42001E, mp4a.40.2\"", "probably");
  check("video/mp4; codecs=\"avc1.58A01E, mp4a.40.2\"", "probably");
  check("video/mp4; codecs=\"avc1.4D401E, mp4a.40.2\"", "probably");
  check("video/mp4; codecs=\"avc1.64001E, mp4a.40.2\"", "probably");
  check("video/mp4; codecs=\"avc1.64001F, mp4a.40.2\"", "probably");

  check("video/mp4; codecs=\"avc1.42E01E\"", "probably");
  check("video/mp4; codecs=\"avc1.42001E\"", "probably");
  check("video/mp4; codecs=\"avc1.58A01E\"", "probably");
  check("video/mp4; codecs=\"avc1.4D401E\"", "probably");
  check("video/mp4; codecs=\"avc1.64001E\"", "probably");
  check("video/mp4; codecs=\"avc1.64001F\"", "probably");

  check("audio/mp4; codecs=\"mp4a.40.2\"", "probably");
  check("audio/mp4; codecs=mp4a.40.2", "probably");
  check("audio/x-m4a; codecs=\"mp4a.40.2\"", "probably");
  check("audio/x-m4a; codecs=mp4a.40.2", "probably");
}

function check_mp3(v, enabled) {
  function check(type, expected) {
    var ex = enabled ? expected : "";
    is(v.canPlayType(type), ex, type + "='" + ex + "'");
  }

  check("audio/mpeg", "maybe");
  check("audio/mp3", "maybe");

  check("audio/mpeg; codecs=\"mp3\"", "probably");
  check("audio/mpeg; codecs=mp3", "probably");

  check("audio/mp3; codecs=\"mp3\"", "probably");
  check("audio/mp3; codecs=mp3", "probably");
}
