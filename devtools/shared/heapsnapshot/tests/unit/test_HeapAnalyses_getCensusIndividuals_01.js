/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the HeapAnalyses{Client,Worker} can get census individuals.

function run_test() {
  run_next_test();
}

const COUNT = { by: "count", count: true, bytes: true };

const CENSUS_BREAKDOWN = {
  by: "coarseType",
  objects: COUNT,
  strings: COUNT,
  scripts: COUNT,
  other: COUNT,
};

const LABEL_BREAKDOWN = {
  by: "internalType",
  then: COUNT,
};

const MAX_INDIVIDUALS = 10;

add_task(function* () {
  const client = new HeapAnalysesClient();

  const snapshotFilePath = saveNewHeapSnapshot();
  yield client.readHeapSnapshot(snapshotFilePath);
  ok(true, "Should have read the heap snapshot");

  const dominatorTreeId = yield client.computeDominatorTree(snapshotFilePath);
  ok(true, "Should have computed dominator tree");

  const { report } = yield client.takeCensus(snapshotFilePath,
                                             { breakdown: CENSUS_BREAKDOWN },
                                             { asTreeNode: true });
  ok(report, "Should get a report");

  let nodesWithLeafIndicesFound = 0;

  yield* (function* assertCanGetIndividuals(censusNode) {
    if (censusNode.reportLeafIndex !== undefined) {
      nodesWithLeafIndicesFound++;

      const response = yield client.getCensusIndividuals({
        dominatorTreeId,
        indices: DevToolsUtils.isSet(censusNode.reportLeafIndex)
          ? censusNode.reportLeafIndex
          : new Set([censusNode.reportLeafIndex]),
        censusBreakdown: CENSUS_BREAKDOWN,
        labelBreakdown: LABEL_BREAKDOWN,
        maxRetainingPaths: 1,
        maxIndividuals: MAX_INDIVIDUALS,
      });

      dumpn(`response = ${JSON.stringify(response, null, 4)}`);

      equal(response.nodes.length, Math.min(MAX_INDIVIDUALS, censusNode.count),
         "response.nodes.length === Math.min(MAX_INDIVIDUALS, censusNode.count)");

      let lastRetainedSize = Infinity;
      for (let individual of response.nodes) {
        equal(typeof individual.nodeId, "number",
              "individual.nodeId should be a number");
        ok(individual.retainedSize <= lastRetainedSize,
           "individual.retainedSize <= lastRetainedSize");
        lastRetainedSize = individual.retainedSize;
        ok(individual.shallowSize, "individual.shallowSize should exist and be non-zero");
        ok(individual.shortestPaths, "individual.shortestPaths should exist");
        ok(individual.shortestPaths.nodes, "individual.shortestPaths.nodes should exist");
        ok(individual.shortestPaths.edges, "individual.shortestPaths.edges should exist");
        ok(individual.label, "individual.label should exist");
      }
    }

    if (censusNode.children) {
      for (let child of censusNode.children) {
        yield* assertCanGetIndividuals(child);
      }
    }
  }(report));

  equal(nodesWithLeafIndicesFound, 4, "Should have found a leaf for each coarse type");

  client.destroy();
});
