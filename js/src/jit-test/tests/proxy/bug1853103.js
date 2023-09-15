let { object, transplant } = transplantableObject();
transplant(newGlobal({newCompartment: true}));
nukeAllCCWs()
try {
  transplant(this)
} catch {}
