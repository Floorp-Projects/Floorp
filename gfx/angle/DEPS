deps = {
  "trunk/third_party/gyp":
      "http://gyp.googlecode.com/svn/trunk@1080",
}

hooks = [
  {
    # A change to a .gyp, .gypi, or to GYP itself should run the generator.
    "pattern": ".",
    "action": ["python", "trunk/build/gyp_angle"],
  },
]
