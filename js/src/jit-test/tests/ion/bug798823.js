function g() {
  switch (0) {
  default:
    w = newGlobal('');
  }
  return function(f, code) {
    try {
      evalcx(code, w)
    } catch (e) {}
  }
}

function f(code) {
  h(Function(code), code);
}
h = g()
f("\
    x = [];\
    y = new Set;\
    z = [];\
    Object.defineProperty(x, 5, {\
        get: (function(j) {}),\
    });\
    Object.defineProperty(z, 3, {});\
    z[9] = 1;\
    x.shift();\
");
f("\
    z.every(function() {\
        x.filter(function(j) {\
            if (j) {} else {\
                y.add()\
            }\
        });\
        return 2\
    })\
");
