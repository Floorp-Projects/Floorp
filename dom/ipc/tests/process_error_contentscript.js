Components.utils.import("resource://gre/modules/ctypes.jsm");

privateNoteIntentionalCrash();

var zero = new ctypes.intptr_t(8);
var badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
var crash = badptr.contents;
