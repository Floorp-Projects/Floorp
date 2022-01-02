var whoCaught = "nobody"

function* wrapNoThrow() {
    let iter = {
      [Symbol.iterator]() {
        return this;
      },
      next() {
        return { value: 10, done: false };
      },
      return() { throw "nonsense"; }
    };
    for (const i of iter)
      yield i;
  }
function foo() {
    try {
	l2:
	for (j of wrapNoThrow()) {
	    try {
		for (i of [1,2,3]) {
		    try {
			break l2;
		    } catch(e) {
			whoCaught = "inner"
		    }
		}
	    } catch (e) {
		whoCaught = "inner2"
	    }
	}
    } catch (e) {
	whoCaught = "correct"
    }
}

try {
    foo();
} catch (e) { whoCaught = "outer" }


assertEq(whoCaught, "correct");
