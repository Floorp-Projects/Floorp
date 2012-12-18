function check_mp4(v, enabled) {
  function check(type, expected) {
    var ex = enabled ? expected : "";
    is(v.canPlayType(type), ex, type + "='" + ex + "'");
  }

  check("video/mp4", "maybe");
  check("audio/mp4", "maybe");
  check("audio/mpeg", "maybe");

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
}
