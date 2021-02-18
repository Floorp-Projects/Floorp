function bar() {}

function foo() {
    const v2 = [0,0];
    v2[16777217] = "a";
    v2.length = 2;
    bar(...v2);
}

for (var i = 0; i < 100; i++) {
    foo();
}
