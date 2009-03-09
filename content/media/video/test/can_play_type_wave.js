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

  // Maybe-supported Wave codecs
  check("audio/wave; codecs=0", "maybe");
  check("audio/wave; codecs=\"0, 1\"", "maybe");

  // Unsupported Wave codecs
  check("audio/wave; codecs=2", "no");
  check("audio/wave; codecs=xyz,0", "no");
  check("audio/wave; codecs=0,xyz", "no");
  check("audio/wave; codecs=\"xyz, 1\"", "no");
  // empty codec names
  check("audio/wave; codecs=,", "no");
  check("audio/wave; codecs=\"0, 1,\"", "no");
}
