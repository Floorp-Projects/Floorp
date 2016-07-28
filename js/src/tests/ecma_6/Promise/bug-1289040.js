if (!this.Promise) {
    this.reportCompare && reportCompare(true,true);
    quit(0);
}

var global = newGlobal();
Promise.prototype.then = global.Promise.prototype.then;
p1 = new Promise(function f(r) {
    r(1);
});
p2 = p1.then(function g(){});

this.reportCompare && reportCompare(true,true);
