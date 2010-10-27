
function Bext(k) {
    if (k > 0) {
        let i = k + 1;
        if (k == 10) {
            function x () { i = 2; }
	}
        Bext(i - 2);
        Bext(i - 2);
    }
    return 0;
}

function f() {
    Bext(12);
}

f();

/* Don't assert. */

