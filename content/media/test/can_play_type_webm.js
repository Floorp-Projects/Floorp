function check_webm(v, enabled) {
  function check(type, expected) {
    is(v.canPlayType(type), enabled ? expected : "no", type);
  }

  // WebM types
  check("video/webm", "maybe");
  check("audio/webm", "maybe");

  // Supported Webm codecs
  check("audio/webm; codecs=vorbis", "probably");
  check("video/webm; codecs=vorbis", "probably");
  check("video/webm; codecs=vorbis,vp8", "probably");
  check("video/webm; codecs=vorbis,vp8.0", "probably");
  check("video/webm; codecs=\"vorbis,vp8\"", "probably");
  check("video/webm; codecs=\"vorbis,vp8.0\"", "probably");
  check("video/webm; codecs=\"vp8, vorbis\"", "probably");
  check("video/webm; codecs=\"vp8.0, vorbis\"", "probably");
  check("video/webm; codecs=vp8", "probably");
  check("video/webm; codecs=vp8.0", "probably");

  // Unsupported WebM codecs
  check("video/webm; codecs=xyz", "");
  check("video/webm; codecs=xyz,vorbis", "");
  check("video/webm; codecs=vorbis,xyz", "");
}
