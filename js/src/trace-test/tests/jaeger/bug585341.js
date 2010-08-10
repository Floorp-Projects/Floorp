// |trace-test| error: invalid arguments
__defineGetter__("x", Float64Array)
Function("\
  with(this) {\
    eval(\"x\")\
  }\
")()
