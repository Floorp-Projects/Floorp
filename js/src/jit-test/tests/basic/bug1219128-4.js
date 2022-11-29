// |jit-test| skip-if: !('oomTest' in this)

x = evalcx("lazy");
oomTest((function() {
    evalcx("({", x);
}))
