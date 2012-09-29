Object.defineProperty(Object.prototype, 'x', {
    set: function() {}
});
var obj = {};
for (var i = 0; i < 100 ; ++i) {
    obj.x = 1;
    delete obj.x;
}
