// The SavedFrame constructor shouldn't have been exposed to JS on the global.

saveStack();
assertEq(typeof SavedFrame, "undefined");
