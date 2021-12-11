// |jit-test| exitstatus: 6;

setInterruptCallback(() => false);
0n == {
  valueOf() {
    interruptIf(true);
    for (;;) {}  // block until interrupt callback, which terminates the script
  }
};
