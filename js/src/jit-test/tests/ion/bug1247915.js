// |jit-test| --ion-pruning=on

evaluate(`
    var i = 0;
    while (!inIon())
        a = [] ? i: () => 5;
`);
