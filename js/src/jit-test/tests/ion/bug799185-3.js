// |jit-test| error: TypeError
function processNode(self) {
    try {
        if (self) return;
        undefined.z;
    } finally {
    }
};
processNode();
