var f = String.prototype.m = function () {
    return [this.m, this];
};
var a = "s".m();
gczeal(4);
evaluate("for (a = 0; a < 13; a++) {}", { noScriptRval: true });
