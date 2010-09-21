var obj = {attr: 'value'};

(function() {
    var name = 'attr';
    for (var i = 0; i < 10; ++i)
        assertEq(obj[name], 'value');
})();

/* Look up a string id. */
