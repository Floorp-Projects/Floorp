if (!('stackTest' in this))
    quit();

stackTest(new Function(`
newGlobal({
  sameZoneAs: []
}).frame;
`));
