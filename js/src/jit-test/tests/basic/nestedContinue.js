x = 10;
outer:
while (x < 10) {
  while (x < 10) {
    if (x < 10)
      continue outer;
    while (x < 10) {
      y = 0;
    }
  }
}
