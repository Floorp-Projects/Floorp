load(libdir + "edges.js");

benchmark("EDGES-PARALLEL-ARRAY-1D", 1, DEFAULT_MEASURE,
          function() {edges1dArraySequentially(ArrInput, Width, Height)},
          function() {edges1dParallelArrayInParallel(ParArrInput1D, Width, Height)});

