// Just don't assert or crash.

function f() {
  let c
  try {
    (eval("\
      (function(){\
        with(\
          this.__defineGetter__(\"x\", function(){for(a = 0; a < 3; a++){c=a}})\
        ){}\
      })\
    "))()
  } catch(e) {}
}
f()
print(x)
