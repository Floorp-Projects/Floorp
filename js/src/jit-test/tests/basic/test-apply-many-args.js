function f(x) {
    if (x == 0)
        return;
    arguments[0]--;
    f.apply(null, arguments);
}

a = [100];
for (var i = 0; i < 2000; ++i)
  a.push(i);
f.apply(null, a);
