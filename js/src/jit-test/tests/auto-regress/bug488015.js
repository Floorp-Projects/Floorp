// Binary: cache/js-dbg-64-e1257570fef8-linux
// Flags:
//
var a = [];
function addEventListener(e, f, g)
{
  a.push(f);
}
function setTimeout(f, t)
{
  a.push(f);
}
var b;
this.document = {};
function e(w) {
  addEventListener("mousedown", d, true);
  function d() {
    var d;
    w.setTimeout(function() {
        b(d);
      }, 0);
  }
  function b(d){
    w.document;  /* Crash Here!! */
  }
}
e(this);
a[0]();
a[1]();
