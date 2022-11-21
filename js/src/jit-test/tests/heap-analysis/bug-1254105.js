// |jit-test| error:Error: Each target must be a GC thing

shortestPaths([, , , undefined], {start: this, maxNumPaths: 5})
