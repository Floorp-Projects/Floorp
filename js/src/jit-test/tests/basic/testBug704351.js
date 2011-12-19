if (this.dis !== undefined) {
    var generatorPrototype = (function() { yield 3; })().__proto__;
    try {
        this.dis(generatorPrototype);
    } catch(e) {}
}
