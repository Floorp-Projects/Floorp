Function.prototype.prototype = function() {}

var type = TypedObject.uint8.array(4).array(4);
var x = new type([
    [, , , 0],
    [, , , 0],
    [, , , 0],
    [, , , 0]
]);

x.map(2, function(y) {
    return 0;
});
