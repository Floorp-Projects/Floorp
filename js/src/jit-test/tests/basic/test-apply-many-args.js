function f(x) {
    if (x == 0)
        return;
    arguments[0]--;
    f.apply(null, arguments);
}

// When the apply-optimization isn't on, each recursive call chews up the C
// stack, so don't push it.
a = [20];

for (var i = 0; i < 2000; ++i)
  a.push(i);
f.apply(null, a);
