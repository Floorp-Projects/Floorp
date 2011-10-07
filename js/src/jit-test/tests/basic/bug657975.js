// |jit-test| debug
setDebug(true);

// bug 657975
function f1(){ "use strict"; options('strict'); }
trap(f1, 0, '')
f1()

// bug 657979
function f2(){ with({a:0}){}; }
trap(f2, 0, '')
f2()

x = 0;

// bug 657984 #1
function f3(){ for(y in x); }
trap(f3, 5, '')
f3()

// bug 657984 #2
function f4(){ for(y in x); }
trap(f4, 8, '')
f4()

// bug 658464
function f5() {
  for ([, x] in 0) {}
}
trap(f5, 7, '')
f5()

// bug 658465
function f6() {
  "use strict";
  print(Math.min(0, 1));
}
trap(f6, 10, '')
f6()

// bug 658491
function f7() {
  try { y = w; } catch(y) {}
}
trap(f7, 14, '')
f7()

// bug 658950
f8 = (function() {
  let x;
  yield
})
trap(f8, 6, undefined);
for (a in f8())
  (function() {})()

// bug 659043
f9 = (function() {
  for (let a = 0; a < 0; ++a) {
    for each(let w in []) {}
  }
})
trap(f9, 23, undefined);
for (b in f9())
  (function() {})()

// bug 659233
f10 = (function() {
    while (h) {
        continue
    }
})
trap(f10, 0, '');
try { f10() } catch (e) {}

// bug 659337
f11 = Function("for (x = 0; x < 6; x++) { gc() }");
trap(f11, 23, '');
f11()
