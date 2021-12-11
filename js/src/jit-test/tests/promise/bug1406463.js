// |jit-test| error:dead object

var P = newGlobal({newCompartment: true}).eval(`
(class extends Promise {
    static resolve(o) {
        return o;
    }
});
`);

Promise.all.call(P, [{
    then(r) {
        nukeAllCCWs();
        r();
    }
}]);
