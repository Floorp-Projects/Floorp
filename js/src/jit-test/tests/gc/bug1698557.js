fullcompartmentchecks(true);
let g = newGlobal({sameZoneAs: this});
for (let i = 0; i < 20; i++) {
  g.Object.prototype.toString;
}
gc();
