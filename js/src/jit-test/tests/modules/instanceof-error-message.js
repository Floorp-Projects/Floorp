function getInstanceOfErrorMessage(x) {
    try {
        var result = {} instanceof x;
    }
    catch (e) {
        return e.message;
    }
}

// Error message for a Module Namespace Exotic Object should be same as normal
// non-callable when using 'instanceof'
import('empty.js').then(
    m => assertEq(getInstanceOfErrorMessage(m),
                  getInstanceOfErrorMessage({})));
