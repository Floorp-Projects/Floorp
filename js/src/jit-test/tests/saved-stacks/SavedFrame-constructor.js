// The SavedFrame constructor shouldn't have been exposed to JS on the global.

assertEq(typeof SavedFrame, "undefined");
