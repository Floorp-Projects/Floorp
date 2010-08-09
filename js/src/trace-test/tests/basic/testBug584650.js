// don't crash
x = (evalcx('lazy'))
x.watch("", function () {})
gczeal(1)
for (w in x) {}

