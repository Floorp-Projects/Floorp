x = 0
for (a = 0; a < HOTLOOP+5; ++a) {
  if (a == HOTLOOP-1) {
    if (!x) {
      __defineSetter__("x", Object.defineProperties)
    }
  }
}
