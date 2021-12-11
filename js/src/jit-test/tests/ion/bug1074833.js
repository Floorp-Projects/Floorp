
var i = 0;
function cond() {
  return i++ < 20;
}

function inline() {
    ({ b: 1 })
}

function f() {
  do {
    ({ b: 1 })
  } while (cond())  
}

i = 0;
f();

function g() {
  do {
    if (cond()) { }
    ({ b: 1 })
  } while (cond())  
}

i = 0;
g();


function h() {
  do {
    inline();
  } while (cond())  
}

i = 0;
h();


i = 0;
for (i = 0; cond(); i++)
  ({ set: Math.w });

