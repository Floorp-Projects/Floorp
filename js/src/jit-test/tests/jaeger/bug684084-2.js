function Function() {
    try {
    var g = this;
    g.c("evil", eval);
    } catch(b) {}
}
var o0 = Function.prototype;
var f = new Function( (null ) );
