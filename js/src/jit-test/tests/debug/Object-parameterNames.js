load(libdir + 'array-compare.js');

var g = newGlobal();
var dbg = new Debugger(g);
var hits = 0;
dbg.onDebuggerStatement = function (frame) {
    var arr = frame.arguments;
    assertEq(arraysEqual(arr[0].parameterNames, []), true);
    assertEq(arraysEqual(arr[1].parameterNames, ["x"]), true);
    assertEq(arraysEqual(arr[2].parameterNames,
                         ["a","b","c","d","e","f","g","h","i","j","k","l","m",
                          "n","o","p","q","r","s","t","u","v","w","x","y","z"]),
             true);
    assertEq(arraysEqual(arr[3].parameterNames, ["a", (void 0), (void 0)]), true);
    assertEq(arr[4].parameterNames, (void 0));
    assertEq(arraysEqual(arr[5].parameterNames, [(void 0), (void 0)]), true);
    assertEq(arraysEqual(arr[6].parameterNames, ["a", "b", "c"]), true);
    assertEq(arraysEqual(arr[7].parameterNames, ["d", "e", "f"]), true);
    assertEq(arr.length, 8);
    hits++;
};

g.eval("("
       + function () {
           (function () { debugger; }
            (function () {},
             function (x) {},
             function (a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z) {},
             function (a, [b, c], {d, e:f}) { },
             {a:1},
             Math.atan2,
             async function (a, b, c) {},
             async function* (d, e, f) {}
            ));
       }
       +")()");
assertEq(hits, 1);
