// |jit-test| error:TypeError

delete String.prototype.indexOf;

function enterFunc (funcName) {
    funcName.indexOf();
}
enterFunc(new Array("foo"));
enterFunc(new String("Foo"));
