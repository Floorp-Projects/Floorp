// Binary: cache/js-dbg-64-bfc259be3fa2-linux
// Flags: -m -n -a
//

if (typeof gcPreserveCode != "function") {
    function gcPreserveCode() {}
}

if (typeof gcslice != "function") {
    function gcslice() {}
}

function f(t)
{
    for (var i = 0; i < 1; ++i) {
        if (typeof(t) != "string") {
        }
    }
}
function m(d)
{
    if (d == 0)
        return "";
    f(m(d - 1));
}
m(1);
gcPreserveCode();
try {
  mjitChunkLimit(1);
} catch(exc1) {}
gcslice(0);
m(1);
gc();
m(2);
