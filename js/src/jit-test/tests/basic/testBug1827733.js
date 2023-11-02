function test(a, b) {
  return a + b;
}

try {
  disnative(test);
} catch (err) {
  assertEq(err.message, "The function hasn't been warmed up, hence no JIT code to disassemble.");
}