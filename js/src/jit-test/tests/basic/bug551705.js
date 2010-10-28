(Function("\
  for each(let x in [\n\
  true,\n\
  (1),\n\
  (1),\n\
  (1),\n\
  (1),\n\
  true,\n\
  true,\n\
  true,\n\
  (1),\n\
  true,\n\
  true,\n\
  (1),\n\
  true,\n\
  true,\n\
  (1),\n\
  (1),\n\
  true,\n\
  true,\n\
  true,\n\
  true\n\
  ]) { \n\
    ((function f(aaaaaa) {\n\
       return aaaaaa.length == 0 ? 0 : aaaaaa[0] + f(aaaaaa.slice(1))\n\
     })([\n\
      x,\n\
      Math.I,\n\
      '',\n\
      null,\n\
      Math.I,\n\
      null,\n\
      new String(),\n\
      new String()\n\
      ]))\n\
}"))()

/* Don't assert/crash. */

