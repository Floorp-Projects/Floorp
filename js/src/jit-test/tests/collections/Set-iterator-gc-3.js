// GC in nested for-loops is safe.

var x;
for (x of new Set([1]))
    for (x of new Set([1]))
        for (x of new Set([1]))
            for (x of new Set([1]))
                for (x of new Set([1]))
                    for (x of new Set([1]))
                        for (x of new Set([1]))
                            for (x of new Set([1]))
                                for (x of new Set([1]))
                                    for (x of new Set([1]))
                                        for (x of new Set([1]))
                                            for (x of new Set([1]))
                                                for (x of new Set([1]))
                                                    for (x of new Set([1]))
                                                        for (x of new Set([1]))
                                                            for (x of new Set([1]))
                                                                gc();
