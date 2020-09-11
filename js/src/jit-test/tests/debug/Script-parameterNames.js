load(libdir + "asserts.js");

var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

function check(expr, expected) {
  let completion = gDO.executeInGlobal(expr);
  if (completion.throw)
    throw completion.throw.unsafeDereference();

  let fn = completion.return;
  if (expected === undefined)
    assertEq(fn.script.parameterNames, undefined);
  else
    assertDeepEq(fn.script.parameterNames, expected);
}

check('(function () {})', []);
check('(function (x) {})', ["x"]);
check('(function (x = 1) {})', ["x"]);
check('(function (a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) {})',
      ["a","b","c","d","e","f","g","h","i","j","k","l","m",
       "n","o","p","q","r","s","t","u","v","w","x","y","z"]);
check('(function (a, [b, c], {d, e:f}) { })',
      ["a", undefined, undefined]);
check('(async function (a, b, c) {})', ["a", "b", "c"]);
check('(async function* (d, e, f) {})', ["d", "e", "f"]);

// Non-function scripts have |undefined| parameterNames.
var log = [];
dbg.onEnterFrame = function(frame) {
  log.push(frame.script.parameterNames);
};
g.evaluate("function foo(a, b) { return eval('1'); }; foo();");
assertDeepEq(log, [undefined, ["a", "b"], undefined]); // global, function, eval
