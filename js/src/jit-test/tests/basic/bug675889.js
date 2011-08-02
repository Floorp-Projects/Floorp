function checkMethods(proto) {
    var names = Object.getOwnPropertyNames(proto);
    for (var i = 0; i < names.length; i++) {
        var name = names[i];
        var prop = proto[name];
    }
}
checkMethods(Function.prototype);
