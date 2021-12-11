function f() {};
function g(o) {
    f = new Function("");
    eval("");
}
g({});
g({});
f++;
