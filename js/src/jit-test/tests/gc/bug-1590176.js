// Create function clones in a separate Zone so that the GC can relazify them.
let g = newGlobal({newCompartment: true});
g.evaluate(`
    function factory() {
        return function() { };
    }
`);

// Delazify, Relazify, Delazify
g.factory()();
finishgc();
startgc(0, 'shrinking');
g.factory()();
