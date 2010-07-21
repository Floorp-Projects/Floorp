deps = {
  "trunk/third_party/gyp":
      "http://gyp.googlecode.com/svn/trunk@800",
}

deps_os = {
  "win": {
    # Cygwin is required for gyp actions, flex, and bison.
    "trunk/third_party/cygwin":
      "http://src.chromium.org/svn/trunk/deps/third_party/cygwin@11984",
  }
}

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_angle"],
  },
]
