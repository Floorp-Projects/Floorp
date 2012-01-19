// |jit-test| error: ReferenceError
mjitChunkLimit(5);
eval("\
try { \
  let (t1 = x) {}\
}  finally {}");
