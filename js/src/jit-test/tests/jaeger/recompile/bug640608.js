try {
{
    function x() {}
}
o = (0).__proto__;
function f(o) {
    o._("", function() {})
}
f(o)
} catch (e) {}
