rex = RegExp("()()()()()()()()()()(z)?(y)");
a = ["sub"];
a[230] = '' + "a"
f = Function.apply(null, a);
"xyz".replace(rex, f);
