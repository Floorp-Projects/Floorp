// |jit-test| slow

if (typeof evalInWorker == "undefined")
    quit();

gcslice(11);
evalInWorker("print('helo world');");
for (i = 0; i < 100000; i++) {}

evalInWorker("\
  for (var i = 0; i < 10; i++) { \
    var o = {}; \
    for (var j = 0; j < 100; j++) \
      o['a' + j] = j; \
    JSON.stringify(o); \
    o = null; \
    gc(); \
}");
for (var i = 0; i < 10; i++) {
    gc();
    for (var j = 0; j < 100000; j++) {}
}
