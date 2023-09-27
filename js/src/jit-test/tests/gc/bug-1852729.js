const g = newGlobal({newCompartment: true});
const domObj = this.transplantableObject().object;
const bar = new g.WeakRef(domObj);
bar.deref();
this.nukeAllCCWs();
