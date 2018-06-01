/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// HeapSnapshot.prototype.takeCensus breakdown: check error handling on property
// gets.
//
// Ported from js/src/jit-test/tests/debug/Memory-takeCensus-07.js

function run_test() {
  const g = newGlobal();
  const dbg = new Debugger(g);

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { get by() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "count", get count() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "count", get bytes() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "objectClass", get then() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "objectClass", get other() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "coarseType", get objects() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "coarseType", get scripts() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "coarseType", get strings() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "coarseType", get other() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  assertThrows(() => {
    saveHeapSnapshotAndTakeCensus(dbg, {
      breakdown: { by: "internalType", get then() {
        throw Error("ಠ_ಠ");
      } }
    });
  }, "ಠ_ಠ");

  do_test_finished();
}
