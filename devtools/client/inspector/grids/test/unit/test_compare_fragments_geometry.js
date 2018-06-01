/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { compareFragmentsGeometry } = require("devtools/client/inspector/grids/utils/utils");

const TESTS = [{
  desc: "No fragments",
  grids: [[], []],
  expected: true
}, {
  desc: "Different number of fragments",
  grids: [
    [{}, {}, {}],
    [{}, {}]
  ],
  expected: false
}, {
  desc: "Different number of columns",
  grids: [
    [{cols: {lines: [{}, {}]}, rows: {lines: []}}],
    [{cols: {lines: [{}]}, rows: {lines: []}}]
  ],
  expected: false
}, {
  desc: "Different number of rows",
  grids: [
    [{cols: {lines: [{}, {}]}, rows: {lines: [{}]}}],
    [{cols: {lines: [{}, {}]}, rows: {lines: [{}, {}]}}]
  ],
  expected: false
}, {
  desc: "Different number of rows and columns",
  grids: [
    [{cols: {lines: [{}]}, rows: {lines: [{}]}}],
    [{cols: {lines: [{}, {}]}, rows: {lines: [{}, {}]}}]
  ],
  expected: false
}, {
  desc: "Different column sizes",
  grids: [
    [{cols: {lines: [{start: 0}, {start: 500}]}, rows: {lines: []}}],
    [{cols: {lines: [{start: 0}, {start: 1000}]}, rows: {lines: []}}]
  ],
  expected: false
}, {
  desc: "Different row sizes",
  grids: [
    [{cols: {lines: [{start: 0}, {start: 500}]}, rows: {lines: [{start: -100}]}}],
    [{cols: {lines: [{start: 0}, {start: 500}]}, rows: {lines: [{start: 0}]}}]
  ],
  expected: false
}, {
  desc: "Different row and column sizes",
  grids: [
    [{cols: {lines: [{start: 0}, {start: 500}]}, rows: {lines: [{start: -100}]}}],
    [{cols: {lines: [{start: 0}, {start: 505}]}, rows: {lines: [{start: 0}]}}]
  ],
  expected: false
}, {
  desc: "Complete structure, same fragments",
  grids: [
    [{cols: {lines: [{start: 0}, {start: 100.3}, {start: 200.6}]},
      rows: {lines: [{start: 0}, {start: 1000}, {start: 2000}]}}],
    [{cols: {lines: [{start: 0}, {start: 100.3}, {start: 200.6}]},
      rows: {lines: [{start: 0}, {start: 1000}, {start: 2000}]}}]
  ],
  expected: true
}];

function run_test() {
  for (const { desc, grids, expected } of TESTS) {
    if (desc) {
      info(desc);
    }
    equal(compareFragmentsGeometry(grids[0], grids[1]), expected);
  }
}
