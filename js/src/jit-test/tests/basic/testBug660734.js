if (Proxy.fix) {
    var handler = { fix: function() { return []; } };
    var p = Proxy.createFunction(handler, function(){}, function(){});
    Proxy.fix(p);
    new p();
}
