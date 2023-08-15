const global1 = newGlobal({"newCompartment": false});
const global2 = newGlobal({"newCompartment": true});

global1.nukeAllCCWs();

const {object, transplant} = this.transplantableObject(global1);

global2.firstGlobalInCompartment(object);
transplant(global1);
