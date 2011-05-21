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
trap(f4, 10, '')
f4()

// bug 658464
function f5() {
  for ([, x] in 0) {}
}
trap(f5, 9, '')
f5()

// bug 658465
function f6() {
  "use strict";
  print(Math.min(0, 1));
}
trap(f6, 10, '')
f6()
