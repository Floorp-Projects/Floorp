for (let name of ["test", Symbol.match, Symbol.replace, Symbol.search]) {
    try {
        RegExp.prototype[name].call({});
    } catch (e) {
        let methodName = typeof name === "symbol" ? `[${name.description}]` : name;
        assertEq(e.message, `${methodName} method called on incompatible Object`);
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
