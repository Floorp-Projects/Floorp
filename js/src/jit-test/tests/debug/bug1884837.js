const v3 = newGlobal({ newCompartment: true, discardSource: true });
Debugger().addDebuggee(v3).createSource(true);
