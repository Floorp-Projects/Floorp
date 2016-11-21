// |jit-test| need-for-each

var obj = { a:1, b:1, c:1, d:1, e:1, f:1, g:1 };
var sum = 0;
for each (var i in obj) {
    sum += i;
    delete obj.f;
}

// this isn't even implemented to work yet; just don't crash or iloop
