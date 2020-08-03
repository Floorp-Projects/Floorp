// |jit-test| error:Error: Each target must be an object, string, or symbol

shortestPaths([, , , undefined], {start: this, maxNumPaths: 5})
