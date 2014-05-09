function checkConstruct(thing, buggy) {
    try {
        new thing();
    } catch (e) {}
}
var boundFunctionPrototype = Function.prototype.bind();
checkConstruct(boundFunctionPrototype, true);
var boundBuiltin = Math.sin.bind();
checkConstruct(boundBuiltin, true);
