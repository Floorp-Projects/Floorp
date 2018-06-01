/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check HeapSnapshot.prototype.takeCensus handling of 'breakdown' argument.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-06.js

function run_test() {
  const Pattern = Match.Pattern;

  const g = newGlobal();
  const dbg = new Debugger(g);

  Pattern({ count: Pattern.NATURAL,
            bytes: Pattern.NATURAL })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: "count" } }));

  let census = saveHeapSnapshotAndTakeCensus(dbg,
    { breakdown: { by: "count", count: false, bytes: false } });
  equal("count" in census, false);
  equal("bytes" in census, false);

  census = saveHeapSnapshotAndTakeCensus(dbg,
    { breakdown: { by: "count", count: true, bytes: false } });
  equal("count" in census, true);
  equal("bytes" in census, false);

  census = saveHeapSnapshotAndTakeCensus(dbg,
    { breakdown: { by: "count", count: false, bytes: true } });
  equal("count" in census, false);
  equal("bytes" in census, true);

  census = saveHeapSnapshotAndTakeCensus(dbg,
    { breakdown: { by: "count", count: true, bytes: true } });
  equal("count" in census, true);
  equal("bytes" in census, true);

  // Pattern doesn't mind objects with extra properties, so we'll restrict this
  // list to the object classes we're pretty sure are going to stick around for
  // the forseeable future.
  Pattern({
    Function: { count: Pattern.NATURAL },
    Object: { count: Pattern.NATURAL },
    Debugger: { count: Pattern.NATURAL },
    Sandbox: { count: Pattern.NATURAL },

            // The below are all Debugger prototype objects.
    Source: { count: Pattern.NATURAL },
    Environment: { count: Pattern.NATURAL },
    Script: { count: Pattern.NATURAL },
    Memory: { count: Pattern.NATURAL },
    Frame: { count: Pattern.NATURAL }
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: "objectClass" } }));

  Pattern({
    objects: { count: Pattern.NATURAL },
    scripts: { count: Pattern.NATURAL },
    strings: { count: Pattern.NATURAL },
    other: { count: Pattern.NATURAL }
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: "coarseType" } }));

  // As for { by: 'objectClass' }, restrict our pattern to the types
  // we predict will stick around for a long time.
  Pattern({
    JSString: { count: Pattern.NATURAL },
    "js::Shape": { count: Pattern.NATURAL },
    JSObject: { count: Pattern.NATURAL },
    JSScript: { count: Pattern.NATURAL }
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, { breakdown: { by: "internalType" } }));

  // Nested breakdowns.

  const coarseTypePattern = {
    objects: { count: Pattern.NATURAL },
    scripts: { count: Pattern.NATURAL },
    strings: { count: Pattern.NATURAL },
    other: { count: Pattern.NATURAL }
  };

  Pattern({
    JSString: coarseTypePattern,
    "js::Shape": coarseTypePattern,
    JSObject: coarseTypePattern,
    JSScript: coarseTypePattern,
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "internalType",
                   then: { by: "coarseType" }
      }
    }));

  Pattern({
    Function: { count: Pattern.NATURAL },
    Object: { count: Pattern.NATURAL },
    Debugger: { count: Pattern.NATURAL },
    Sandbox: { count: Pattern.NATURAL },
    other: coarseTypePattern
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: {
        by: "objectClass",
        then: { by: "count" },
        other: { by: "coarseType" }
      }
    }));

  Pattern({
    objects: { count: Pattern.NATURAL, label: "object" },
    scripts: { count: Pattern.NATURAL, label: "scripts" },
    strings: { count: Pattern.NATURAL, label: "strings" },
    other: { count: Pattern.NATURAL, label: "other" }
  })
    .assert(saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: {
        by: "coarseType",
        objects: { by: "count", label: "object" },
        scripts: { by: "count", label: "scripts" },
        strings: { by: "count", label: "strings" },
        other: { by: "count", label: "other" }
      }
    }));

  do_test_finished();
}
