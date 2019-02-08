(new (newGlobal({newCompartment:true}).Debugger)(this)).memory.trackingAllocationSites = true;
nukeAllCCWs();
new Date();
