// |jit-test| slow; allow-oom;

// Binary: cache/js-dbg-64-dfcb11712ec2-linux
// Flags:
//

a=[];
for (var i=0; i<10; i++) {
  a[a.length] = a;
}
try {
  for (var i=0; i<26; i++) {
    a[a.length] = a.toString();
  }
} catch(exc1) {}
