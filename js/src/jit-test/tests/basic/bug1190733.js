
x = [];
Array.prototype.push.call(x, Uint8ClampedArray);
(function() {
    x.length = 9;
})();
Array.prototype.reverse.call(x);
