const v1 = ["foo"];
const v17 = {...this};
const glob = newGlobal({sameZoneAs: this});
const dbg = glob.Debugger({});
const v0 = eval(`
  const frame = dbg.getNewestFrame();
  with (frame) {
    for (const v13 in eval("function f9() {}")) {}
  }
  const v15 = [v1];
  findPath(v15, v15);
`);
JSON.stringify(v0);
