assertEq((function() { var x = 3; return (function() { return x } )() })(), 3);
assertEq((function() { var g = function() { return x }; var x = 3; return g()})(), 3);
assertEq((function() { var x; x = 3; return (function() { return x } )() })(), 3);
assertEq((function() { x = 3; var x; return (function() { return x } )() })(), 3);
assertEq((function() { var x; var g = function() { return x }; x = 3; return g() })(), 3);

assertEq((function() { function f() { return 3 }; assertEq(f(), 3); return (function() f())() })(), 3);
assertEq((function() { function f() { return 3 }; assertEq(f(), 3); return eval('f()') })(), 3);
assertEq((function() { function f() { return 3 }; (function() f())(); return f() })(), 3);

assertEq((function() { const x = 3; (function() x++)(); return x })(), 3);
assertEq((function() { const x = 3; (function() x++)(); return x++ })(), 3);
assertEq((function() { const x = 2; (function() x++)(); return ++x })(), 3);
assertEq((function() { const x = 2; (function() x++)(); return x += 1 })(), 3);

assertEq((function() { const x = 3; x = 1; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; x = 1; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; x++; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; ++x; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; x--; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; --x; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; x += 1; return (function() { return x})() })(), 3);
assertEq((function() { const x = 3; x -= 1; return (function() { return x})() })(), 3);

assertEq((function() { var x = 3; return eval("x") })(), 3);
assertEq((function() { var x; x = 3; return eval("x") })(), 3);
assertEq((function() { x = 3; var x; return eval("x") })(), 3);

assertEq((function() { var x; (function() {x = 2})(); return ++x })(), 3);
assertEq((function() { var x; (function() {x = 2})(); x++; return x })(), 3);
assertEq((function() { var x; (function() {x = 4})(); return --x })(), 3);
assertEq((function() { var x; (function() {x = 4})(); x--; return x })(), 3);

assertEq((function(x) { return (function() { return x } )() })(3), 3);
assertEq((function(x) { var x = 3; return (function() { return x } )() })(4), 3);
assertEq((function(x) { x = 3; return (function() { return x } )() })(4), 3);
assertEq((function(x) { var g = function() { return x }; x = 3; return g()})(3), 3);

assertEq((function(x) { return eval("x") })(3), 3);
assertEq((function(x) { x = 3; return eval("x") })(4), 3);

assertEq((function(a) { var [x,y] = a; (function() { x += y })(); return x })([1,2]), 3);
assertEq((function(a) { var [x,y] = a; x += y; return (function() x)() })([1,2]), 3);
assertEq((function(a) { var [[l, x],[m, y]] = a; x += y; return (function() x)() })([[0,1],[0,2]]), 3);
assertEq((function(a) { var [x,y] = a; eval('x += y'); return x })([1,2]), 3);
assertEq((function(a) { var [x,y] = a; x += y; return eval('x') })([1,2]), 3);
assertEq((function(a) { var [x,y] = a; (function() { x += y })(); return x })([1,2]), 3);
assertEq((function(a) { var [x,y] = a; x += y; return (function() x)() })([1,2]), 3);
assertEq((function(a,x,y) { [x,y] = a; (function() { eval('x += y') })(); return x })([1,2]), 3);
assertEq((function(a,x,y) { [x,y] = a; x += y; return (function() eval('x'))() })([1,2]), 3);

assertEq((function() { var [x,y] = [1,2]; x += y; return (function() x)() })(), 3);
assertEq((function() { var [x,y] = [1,2]; (function() x += y)(); return x })(), 3);
assertEq((function() { let ([x,y] = [1,2]) { x += y; return (function() x)() } })(), 3);
assertEq((function() { let ([x,y] = [1,2]) { (function() x += y)(); return x } })(), 3);

assertEq((function([x,y]) { (function() { x += y })(); return x })([1,2]), 3);
assertEq((function([x,y]) { x += y; return (function() x)() })([1,2]), 3);
assertEq((function([[l,x],[m,y]]) { (function() { x += y })(); return x })([[0,1],[0,2]]), 3);
assertEq((function([[l,x],[m,y]]) { x += y; return (function() x)() })([[0,1],[0,2]]), 3);
assertEq((function([x,y]) { (function() { eval('x += y') })(); return x })([1,2]), 3);
assertEq((function([x,y]) { x += y; return (function() eval('x'))() })([1,2]), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { eval('x += y'); return x }})(), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { x += y; return eval('x') }})(), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { (function() { x += y })(); return x }})(), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { x += y; return (function() x)() }})(), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { (function() { eval('x += y') })(); return x }})(), 3);
assertEq((function() { try { throw [1,2] } catch([x,y]) { x += y; return (function() eval('x'))() }})(), 3);

assertEq((function(a) { let [x,y] = a; (function() { x += y })(); return x })([1,2]), 3);
assertEq((function(a) { let [x,y] = a; x += y; return (function() x)() })([1,2]), 3);
assertEq((function(a) { let ([x,y] = a) { (function() { x += y })(); return x } })([1,2]), 3);
assertEq((function(a) { let ([x,y] = a) { x += y; return (function() x)() } })([1,2]), 3);
assertEq((function(a) { let ([[l, x],[m, y]] = a) { (function() { x += y })(); return x } })([[0,1],[0,2]]), 3);
assertEq((function(a) { let ([[l, x],[m, y]] = a) { x += y; return (function() x)() } })([[0,1],[0,2]]), 3);
assertEq((function(a) { return let ([x,y] = a) ((function() { x += y })(), x) })([1,2]), 3);
assertEq((function(a) { return let ([x,y] = a) (x += y, (function() x)()) })([1,2]), 3);
assertEq((function(a) { return let ([[l, x],[m, y]] = a) ((function() { x += y })(), x) })([[0,1],[0,2]]), 3);
assertEq((function(a) { return let ([[l, x],[m, y]] = a) (x += y, (function() x)()) })([[0,1],[0,2]]), 3);

assertEq((function() { let x = 3; return (function() { return x })() })(), 3);
assertEq((function() { let g = function() { return x }; let x = 3; return g() })(), 3);

assertEq((function() { let (x = 3) { return (function() { return x })() } })(), 3);
assertEq((function() { let (x = 3) { (function() { assertEq(x, 3) })(); return x } })(), 3);
assertEq((function() { let (x = 2) { x = 3; return (function() { return x })() } })(), 3);
assertEq((function() { let (x = 1) { let (x = 3) { (function() { assertEq(x,3) })() } return x } })(), 1);

assertEq((function() { try { throw 3 } catch (e) { (function(){assertEq(e,3)})(); return e } })(), 3);
assertEq((function() { try { throw 3 } catch (e) { assertEq(e, 3); return (function() e)() } })(), 3);
assertEq((function() { try { throw 3 } catch (e) { (function(){eval('assertEq(e,3)')})(); return e } })(), 3);

assertEq((function() { return [(function() i)() for (i of [3])][0] })(), 3);
assertEq((function() { return [((function() i++)(), i) for (i of [2])][0] })(), 3);
assertEq((function() { return [(i++, (function() i)()) for (i of [2])][0] })(), 3);

assertEq((function() { var x; function f() { return x } function f() { return 3 }; return f() })(), 3);
assertEq((function() { var x = 3; function f() { return 3 } function f() { return x }; return f() })(), 3);

assertEq((function() { function f(x,y) { (function() { assertEq(f.length, 2) })(); }; return f.length })(), 2);
assertEq((function f(x,y) { (function() { assertEq(f.length, 2) })(); return f.length})(), 2);
function f1(x,y) { (function() { assertEq(f1.length, 2) })(); assertEq(f1.length, 2) }; f1();

String(function([]) { eval('') });
