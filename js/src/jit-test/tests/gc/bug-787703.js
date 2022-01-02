// |jit-test| slow;

eval(" function x() {}" + Array(241).join(" "));
for (var i = 0; i < 100; i++) {
    gczeal(4, 2);
    String(x);
}
