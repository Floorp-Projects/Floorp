assertEq((function(x) {
    (function () { x++ })();
    var z;
    ({ z } = { z:'ponies' })
    return z;
})(), 'ponies');
