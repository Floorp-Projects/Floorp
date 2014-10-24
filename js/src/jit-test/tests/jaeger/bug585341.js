this.__defineGetter__("x", () => new Float64Array())
Function("\
  with(this) {\
    eval(\"x\")\
  }\
")()
