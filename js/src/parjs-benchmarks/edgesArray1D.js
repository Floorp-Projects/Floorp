load(libdir + "edges.js");

benchmark("EDGES-ARRAY-1D", 1, DEFAULT_MEASURE,
          function() {edges1dArraySequentially(ArrInput, Width, Height)},
          function() {edges1dArrayInParallel(ArrInput, Width, Height)});

