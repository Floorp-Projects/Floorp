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

  // Unsupported Ogg codecs
  check("video/ogg; codecs=xyz", "no");
  check("video/ogg; codecs=xyz,vorbis", "no");
  check("video/ogg; codecs=vorbis,xyz", "no");
}
