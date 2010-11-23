/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SMIL Test Code.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    new AnimTestcaseFromBy("0", "50",  { fromComp: "0px", // 0 acts like 0px
                                         midComp:  "25",
                                         toComp:   "50"}),
    new AnimTestcaseFromBy("30", "10", { fromComp: "30",
                                         midComp:  "35",
                                         toComp:   "40"}),
  ],
  lengthPx: [
    new AnimTestcaseFromBy("0", "8px", { fromComp: "0px", // 0 acts like 0px
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
    // XXXdholbert Of course, we don't support animation between URI values yet
    // (bug 520487), so the testcases that use URIs currently have no effect
    // for that reason, too.
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
