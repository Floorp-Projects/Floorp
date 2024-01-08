// |jit-test| slow;

for (var i = 0; i < 32768; i++) {
  new ArrayBuffer(1024*1024);
}
