function check_wave(v, enabled) {
  function check(type, expected) {
    is(v.canPlayType(type), enabled ? expected : "no", type);
  }

  // Wave types
  check("audio/wave", "maybe");
  check("audio/wav", "maybe");
  check("audio/x-wav", "maybe");
  check("audio/x-pn-wav", "maybe");

  // Supported Wave codecs
  check("audio/wave; codecs=1", "probably");
  // "no codecs" should be supported, I guess
  check("audio/wave; codecs=", "probably");
  check("audio/wave; codecs=\"\"", "probably");

  // Unsupported Wave codecs
  check("audio/wave; codecs=0", "");
  check("audio/wave; codecs=2", "");
  check("audio/wave; codecs=xyz,1", "");
  check("audio/wave; codecs=1,xyz", "");
  check("audio/wave; codecs=\"xyz, 1\"", "");
  // empty codec names
  check("audio/wave; codecs=,", "");
  check("audio/wave; codecs=\"0, 1,\"", "");
}
