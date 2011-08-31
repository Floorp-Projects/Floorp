// |jit-test| error: TypeError
new DoWhileObject;
function DoWhileObject(breakOut, breakIn, iterations, loops) {
    loops.prototype = new DoWhile;
    this.looping;
}
function DoWhile(object) {
    do {} while (object);
}
