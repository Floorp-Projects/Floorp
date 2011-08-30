Object.extend = function(destination, source) {
    for (var property in source)
        destination[property] = source[property];
};
var Enumerable = {
    _each: function(iterator) {
        for (var i = 0, length = this.length; i < length; i++)
            iterator(this[i]);
    },
    each: function(iterator, context) {
        var index = 0;
        this._each(function(value) {
            iterator.call(context, value, index++);
        });
    },
    map: function(iterator, context) {
        var results = [];
        this.each(function(value, index) {
            var res = iterator.call(context, value);
            results.push(res);
        });
        return results;
    },
    invoke: function(method) {
        var args = $A(arguments).slice(1);
        return this.map(function(value) {
            return value[method].apply(value, args);
        });
    },
};
Object.extend(Array.prototype, Enumerable);
function $A(iterable) {
    var length = iterable.length || 0, results = new Array(length);
    while (length--) results[length] = iterable[length];
    return results;
}
function g() {
    return [1, 2, 3, 4, 5].each(function(part) {
        return 0;
    });
}
function f() {
    g();
    g();
    g();
    g();
    var result = [[2, 1, 3], [6, 5, 4]];    
    result = result.invoke('invoke', 'toString', 2);
    result[0].join(', ');
};
f();
