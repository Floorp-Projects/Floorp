load(libdir + 'array-compare.js');
load(libdir + 'nightly-only.js');

var g = newGlobal();
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);
var hits = 0;

function check(expr, expected) {
  print("checking " + uneval(expr));

  let completion = gDO.executeInGlobal(expr);
  if (completion.throw)
    throw completion.throw.unsafeDereference();

  let fn = completion.return;
  if (expected === undefined)
    assertEq(fn.parameterNames, undefined);
  else
    assertEq(arraysEqual(fn.parameterNames, expected), true);
}

check('(function () {})', []);
check('(function (x) {})', ["x"]);
check('(function (a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) {})',
      ["a","b","c","d","e","f","g","h","i","j","k","l","m",
       "n","o","p","q","r","s","t","u","v","w","x","y","z"]);
check('(function (a, [b, c], {d, e:f}) { })',
      ["a", undefined, undefined]);
check('({a:1})', undefined);
check('Math.atan2', [undefined, undefined]);
check('(async function (a, b, c) {})', ["a", "b", "c"]);
nightlyOnly(g.SyntaxError, () => {
  check('(async function* (d, e, f) {})', ["d", "e", "f"]);
});
