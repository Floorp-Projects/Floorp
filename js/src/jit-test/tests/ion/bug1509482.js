let singleton = {x: 1};

let holder = {sing_prop: singleton}

function makeChain(n, base) {
    var curr = base;
    for (var i = 0; i < n; i++) {
	curr = Object.create(curr);
    }
    return curr;
}
let chain = makeChain(1000, holder);

var x = 0;
for (var i = 0; i < 1111; i++) {
    x += chain.sing_prop.x;
    singleton.x = -singleton.x // Don't want it to be a constant.
}
