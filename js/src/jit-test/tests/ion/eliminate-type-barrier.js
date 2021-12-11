function foo(a) {
  var y = 0;
  for (var i = 0; i < 10; i++) {
    var x = a[i];
    z = x.f;
    if (x.h != null)
      y = x.f.g;
  }
  return y;
}

function foo2(a) {
  var y = 0;
  for (var i = 0; i < 10; i++) {
    var x = a[i];
    if (x.f !== undefined) {
      if (x.h != null)
        y = x.f.g;
    }
  }
  return y;
}

a = [];
for (var i = 0; i < 10; i++)
  a[i] = {f:null, h:null};
for (var i = 0; i < 10; i++) {
  a[i].f = {g:0};
  a[i].h = {};
}
var q = a[0].h;
a[0].f = null;
a[0].h = null;

foo(a);
foo2(a);

a[0].h = q;

try { foo(a); } catch (e) {}
try { foo2(a); } catch (e) {}
