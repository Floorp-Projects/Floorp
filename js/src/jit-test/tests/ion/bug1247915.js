// |jit-test| --ion-pgo=on

evaluate(`
    var i = 0;
    while (!inIon())
        a = [] ? i: () => 5;
`);
