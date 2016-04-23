if (!('oomTest' in this))
    quit();
function g(f, params) {
    entryPoints(params);
}
function entry1() {};
s = "g(entry1, {function: entry1});";
f(s);
f(s);
function f(x) {
    oomTest(() => eval(x));
}
