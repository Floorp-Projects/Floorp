load(libdir + "edges.js");

benchmark("EDGES-PARALLEL-ARRAY-2D", 1, DEFAULT_MEASURE,
          function() {edges1dArraySequentially(ArrInput, Width, Height)},
          function() {edges2dParallelArrayInParallel(ParArrInput2D)});
