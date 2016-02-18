// |jit-test| error: TypeError

newGlobal().Debugger(this).onExceptionUnwind = function() {
    return {
        // Force the return of an illegal value.
        return: 1
    }
}

class forceException extends class {} {
    // An explicit exception to trigger unwinding.
    // This function will throw for having an uninitialized this.
    constructor() {}
}
new forceException;
