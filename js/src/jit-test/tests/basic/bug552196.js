(Function("\
  for (a = 0; a < 5; a++)\n\
  (function f(b) {\n\
    if (b > 0) {\n\
      f(b - 1)\n\
    }\n\
  })\n\
  (3)\n\
"))()

/* Don't assert. */

