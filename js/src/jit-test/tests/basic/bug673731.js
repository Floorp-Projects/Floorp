// |jit-test| error: ReferenceError

const IS_TOKEN_ARRAY = [ printBugNumber && this() ? this() : this() ];
