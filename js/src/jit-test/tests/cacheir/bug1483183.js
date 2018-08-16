stackTest(new Function(`
newGlobal({
  sameZoneAs: []
}).frame;
`));
