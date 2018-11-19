function makeChain(n, base) {
    var curr = base;
    for (var i = 0; i < n; i++) {
	curr = Object.create(curr);
	var propname = "level" + i;
	curr[propname] = true;
    }
    return curr;
}

function BaseClass() {
    this.base = true;
}

Object.defineProperty(BaseClass.prototype, "getter", {get: function() { with({}){}; return this.base; }});

function victim(arg) {
    if (arg.getter) {
	return 3;
    } else {
	return 4;
    }
}

let root = new BaseClass();
let chains = [];
for (var i = 0; i < 6; i++) {
    chains.push(makeChain(500, root));
}

with({}){};
for (var i = 0; i < 1000 / 6; i++) {
    with({}){};
    for (var j = 0; j < chains.length; j++) {
	victim(chains[j]);
    }
}
