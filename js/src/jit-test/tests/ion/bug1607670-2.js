function inner(X, T) {
    return Reflect.construct(X, [], T);
}
function F() {}

let handler = {};
let P = new Proxy(F, handler);

for (var i = 0; i < 2000; i += 1) {
    with ({}) {}
    inner(F, P);
}

handler.get = function() {}
inner(F, P);
