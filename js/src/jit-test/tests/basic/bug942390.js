var count = 0;
Object.defineProperty(__proto__, "__iterator__", {
    get: (function() {
	count++;
    })
})
__proto__ = (function(){})
for (var m = 0; m < 7; ++m) {
    for (var a in 6) {}
}
assertEq(count, 7);
