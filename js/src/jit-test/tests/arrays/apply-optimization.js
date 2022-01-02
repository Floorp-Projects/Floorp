function make(k) {
    var a = new Array(k);
    for ( let i=0 ; i < k ; i++ )
	a[i] = {}
    return a;
}

function g() {
    return arguments.length;
}

function f(a) {
    var sum = 0;
    for ( let i=0 ; i < 1000 ; i++ )
	sum += g.apply(null, a);
    return sum;
}

function F2() {
    var sum = 0;
    for ( let i=0 ; i < 1000 ; i++ )
	sum += g.apply(null, arguments);
    return sum;
}

function F(a) {
    return F2.apply(null, a);
}

function time(k, t) {
    var then = Date.now();
    assertEq(t(), 1000*k);
    var now = Date.now();
    return now - then;
}

function p(v) {
    // Uncomment to see timings
    // print(v);
}

f(make(200));

// There is currently a cutoff after 375 where we bailout in order to avoid
// handling very large stack frames.  This slows the operation down by a factor
// of 100 or so.

p(time(374, () => f(make(374))));
p(time(375, () => f(make(375))));
p(time(376, () => f(make(376))));
p(time(377, () => f(make(377))));

F(make(200));

p(time(374, () => F(make(374))));
p(time(375, () => F(make(375))));
p(time(376, () => F(make(376))));
p(time(377, () => F(make(377))));
