// GC in nested for-loops is safe.

var x;
for (x of Set([1]))
    for (x of Set([1]))
        for (x of Set([1]))
            for (x of Set([1]))
                for (x of Set([1]))
                    for (x of Set([1]))
                        for (x of Set([1]))
                            for (x of Set([1]))
                                for (x of Set([1]))
                                    for (x of Set([1]))
                                        for (x of Set([1]))
                                            for (x of Set([1]))
                                                for (x of Set([1]))
                                                    for (x of Set([1]))
                                                        for (x of Set([1]))
                                                            for (x of Set([1]))
                                                                gc();
