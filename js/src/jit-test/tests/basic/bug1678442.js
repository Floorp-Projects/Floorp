var fun = function() {};
var newTarget = (function(){}).bind();
Object.defineProperty(newTarget, "prototype", {get() { relazifyFunctions() }});
for (var i = 0; i < 10; i++) {
    Reflect.construct(fun, [], newTarget);
}
