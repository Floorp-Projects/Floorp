if (!('oomTest' in this)) {
    quit();
}

function foo() {}
function foooooooooooooooooooooooooooooooo() {}
function fn(s) {
    var o = {a:1}
    eval(("f" + s) + "()");
    if (!('a' in o)) {
        print("unreachable");
    }
}
for (var i = 0; i < 1100; i++) {
    fn("oo");
}
oomTest(new Function(`
  let a = newRope("oooooooooooooooo","oooooooooooooooo");
  fn(a);
`))
