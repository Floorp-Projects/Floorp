x = evalcx("lazy");
oomTest(function() {
    x.of(new(delete y));
});
