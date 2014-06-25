/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* testcase data for simple "from-by" animations of CSS properties */

// NOTE: This js file requires db_smilCSSPropertyList.js

// Lists of testcases for re-use across multiple properties of the same type
var _fromByTestLists =
{
  color: [
    new AnimTestcaseFromBy("rgb(10, 20, 30)", "currentColor",
                           { midComp: "rgb(35, 45, 55)",
                             toComp:  "rgb(60, 70, 80)"}),
    new AnimTestcaseFromBy("currentColor", "rgb(30, 20, 10)",
                           { fromComp: "rgb(50, 50, 50)",
                             midComp:  "rgb(65, 60, 55)",
                             toComp:   "rgb(80, 70, 60)"}),
  ],
  lengthNoUnits: [
    new AnimTestcaseFromBy("0", "50",  { fromComp: "0px", // 0 acts like 0px
                                         midComp:  "25px",
                                         toComp:   "50px"}),
    new AnimTestcaseFromBy("30", "10", { fromComp: "30px",
                                         midComp:  "35px",
                                         toComp:   "40px"}),
  ],
  lengthNoUnitsSVG: [
    new AnimTestcaseFromBy("0", "50",  { fromComp: "0",
                                         midComp:  "25",
                                         toComp:   "50"}),
    new AnimTestcaseFromBy("30", "10", { fromComp: "30",
                                         midComp:  "35",
                                         toComp:   "40"}),
  ],
  lengthPx: [
    new AnimTestcaseFromBy("0px", "8px", { fromComp: "0px",
                                           midComp: "4px",
                                           toComp: "8px"}),
    new AnimTestcaseFromBy("1px", "10px", { midComp: "6px", toComp: "11px"}),
  ],
  opacity: [
    new AnimTestcaseFromBy("1", "-1", { midComp: "0.5", toComp: "0"}),
    new AnimTestcaseFromBy("0.4", "-0.6", { midComp: "0.1", toComp: "0"}),
    new AnimTestcaseFromBy("0.8", "-1.4", { midComp: "0.1", toComp: "0"},
                           "opacities with abs val >1 get clamped too early"),
    new AnimTestcaseFromBy("1.2", "-0.6", { midComp: "0.9", toComp: "0.6"},
                           "opacities with abs val >1 get clamped too early"),
  ],
  paint: [
    // The "none" keyword & URI values aren't addiditve, so the animations in
    // these testcases are expected to have no effect.
    new AnimTestcaseFromBy("none", "none",  { noEffect: 1 }),
    new AnimTestcaseFromBy("url(#gradA)", "url(#gradB)", { noEffect: 1 }),
    new AnimTestcaseFromBy("url(#gradA)", "url(#gradB) red", { noEffect: 1 }),
    new AnimTestcaseFromBy("url(#gradA)", "none", { noEffect: 1 }),
    new AnimTestcaseFromBy("red", "url(#gradA)", { noEffect: 1 }),
  ],
  URIsAndNone: [
    // No need to specify { noEffect: 1 }, since plain URI-valued properties
    // aren't additive
    new AnimTestcaseFromBy("url(#idA)", "url(#idB)"),
    new AnimTestcaseFromBy("none", "url(#idB)"),
    new AnimTestcaseFromBy("url(#idB)", "inherit"),
  ],
};

// List of attribute/testcase-list bundles to be tested
var gFromByBundles =
[
  new TestcaseBundle(gPropList.clip, [
    new AnimTestcaseFromBy("rect(1px, 2px, 3px, 4px)",
                           "rect(10px, 20px, 30px, 40px)",
                           { midComp: "rect(6px, 12px, 18px, 24px)",
                             toComp:  "rect(11px, 22px, 33px, 44px)"}),
    // Adding "auto" (either as a standalone value or a subcomponent value)
    // should cause animation to fail.
    new AnimTestcaseFromBy("auto", "auto", { noEffect: 1 }),
    new AnimTestcaseFromBy("auto",
                           "rect(auto, auto, auto, auto)", { noEffect: 1 }),
    new AnimTestcaseFromBy("rect(auto, auto, auto, auto)",
                           "rect(auto, auto, auto, auto)", { noEffect: 1 }),
    new AnimTestcaseFromBy("rect(1px, 2px, 3px, 4px)", "auto", { noEffect: 1 }),
    new AnimTestcaseFromBy("auto", "rect(1px, 2px, 3px, 4px)", { noEffect: 1 }),
    new AnimTestcaseFromBy("rect(1px, 2px, 3px, auto)",
                           "rect(10px, 20px, 30px, 40px)", { noEffect: 1 }),
    new AnimTestcaseFromBy("rect(1px, auto, 3px, 4px)",
                           "rect(10px, auto, 30px, 40px)", { noEffect: 1 }),
    new AnimTestcaseFromBy("rect(1px, 2px, 3px, 4px)",
                           "rect(10px, auto, 30px, 40px)", { noEffect: 1 }),
  ]),
  // Check that 'by' animations for 'cursor' has no effect
  new TestcaseBundle(gPropList.cursor, [
    new AnimTestcaseFromBy("crosshair", "move"),
  ]),
  new TestcaseBundle(gPropList.fill, [].concat(_fromByTestLists.color,
                                               _fromByTestLists.paint)),
  // Check that 'by' animations involving URIs have no effect
  new TestcaseBundle(gPropList.filter,         _fromByTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.font, [
    new AnimTestcaseFromBy("10px serif",
                           "normal normal 400 100px / 10px monospace"),
  ]),
  new TestcaseBundle(gPropList.font_size,
                     [].concat(_fromByTestLists.lengthNoUnits,
                               _fromByTestLists.lengthPx)),
  new TestcaseBundle(gPropList.font_size_adjust, [
    // These testcases implicitly have no effect, because font-size-adjust is
    // non-additive (and is declared as such in db_smilCSSPropertyList.js)
    new AnimTestcaseFromBy("0.5", "0.1"),
    new AnimTestcaseFromBy("none", "0.1"),
    new AnimTestcaseFromBy("0.1", "none")
  ]),
  new TestcaseBundle(gPropList.lighting_color, _fromByTestLists.color),
  new TestcaseBundle(gPropList.marker,         _fromByTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_end,     _fromByTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_mid,     _fromByTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_start,   _fromByTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.overflow, [
    new AnimTestcaseFromBy("inherit", "auto"),
    new AnimTestcaseFromBy("scroll", "hidden")
  ]),
  new TestcaseBundle(gPropList.opacity,        _fromByTestLists.opacity),
  new TestcaseBundle(gPropList.stroke_miterlimit, [
    new AnimTestcaseFromBy("1", "1", { midComp: "1.5", toComp: "2" }),
    new AnimTestcaseFromBy("20.1", "-10", { midComp: "15.1", toComp: "10.1" }),
  ]),
  new TestcaseBundle(gPropList.stroke_dasharray, [
    // These testcases implicitly have no effect, because stroke-dasharray is
    // non-additive (and is declared as such in db_smilCSSPropertyList.js)
    new AnimTestcaseFromBy("none", "5"),
    new AnimTestcaseFromBy("10", "5"),
    new AnimTestcaseFromBy("1", "2, 3"),
  ]),
  new TestcaseBundle(gPropList.stroke_width,
                     [].concat(_fromByTestLists.lengthNoUnitsSVG,
                               _fromByTestLists.lengthPx))
];
