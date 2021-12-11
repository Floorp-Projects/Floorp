function f(o) {
    f = o.constructor;
    eval('delete o.x');
}
for(var i=0; i<3; i++) {
    f(RegExp.prototype);
}
