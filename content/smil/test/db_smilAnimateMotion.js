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
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

/* testcase data for <animateMotion> */

// Fake motion 'attribute', to satisfy testing code that expects an attribute.
var gMotionAttr = new AdditiveAttribute(SMILUtil.getMotionFakeAttributeName(),
                                        "XML", "rect");

// CTM-summary-definitions, for re-use by multiple testcase bundles below.
var _reusedCTMLists = {
  pacedBasic:     { ctm0:   [100, 200, 0],
                    ctm1_6: [105, 205, 0],
                    ctm1_3: [110, 210, 0],
                    ctm2_3: [120, 220, 0],
                    ctm1:   [130, 210, 0]
  },
  pacedR60:       { ctm0:   [100, 200, Math.PI/3],
                    ctm1_6: [105, 205, Math.PI/3],
                    ctm1_3: [110, 210, Math.PI/3],
                    ctm2_3: [120, 220, Math.PI/3],
                    ctm1:   [130, 210, Math.PI/3]
  },
  pacedRAuto:     { ctm0:   [100, 200, Math.PI/4],
                    ctm1_6: [105, 205, Math.PI/4],
                    ctm1_3: [110, 210, Math.PI/4],
                    ctm2_3: [120, 220, Math.PI/4],
                    ctm1:   [130, 210, -Math.PI/4]
  },
  pacedRAutoReverse : { ctm0:   [100, 200, 5*Math.PI/4],
                        ctm1_6: [105, 205, 5*Math.PI/4],
                        ctm1_3: [110, 210, 5*Math.PI/4],
                        ctm2_3: [120, 220, 5*Math.PI/4],
                        ctm1:   [130, 210, 3*Math.PI/4]
  },
  
  discreteBasic : { ctm0:   [100, 200, 0],
                    ctm1_6: [100, 200, 0],
                    ctm1_3: [120, 220, 0],
                    ctm2_3: [130, 210, 0],
                    ctm1:   [130, 210, 0]
  },
  discreteRAuto : { ctm0:   [100, 200, Math.PI/4],
                    ctm1_6: [100, 200, Math.PI/4],
                    ctm1_3: [120, 220, Math.PI/4],
                    ctm2_3: [130, 210, -Math.PI/4],
                    ctm1:   [130, 210, -Math.PI/4]
  },
  justMoveBasic : { ctm0:   [40, 80, 0],
                    ctm1_6: [40, 80, 0],
                    ctm1_3: [40, 80, 0],
                    ctm2_3: [40, 80, 0],
                    ctm1:   [40, 80, 0]
  },
  justMoveR60 :   { ctm0:   [40, 80, Math.PI/3],
                    ctm1_6: [40, 80, Math.PI/3],
                    ctm1_3: [40, 80, Math.PI/3],
                    ctm2_3: [40, 80, Math.PI/3],
                    ctm1:   [40, 80, Math.PI/3]
  },
  justMoveRAuto : { ctm0:   [40, 80, Math.atan(2)],
                    ctm1_6: [40, 80, Math.atan(2)],
                    ctm1_3: [40, 80, Math.atan(2)],
                    ctm2_3: [40, 80, Math.atan(2)],
                    ctm1:   [40, 80, Math.atan(2)]
  },
  justMoveRAutoReverse : { ctm0:   [40, 80, Math.PI + Math.atan(2)],
                           ctm1_6: [40, 80, Math.PI + Math.atan(2)],
                           ctm1_3: [40, 80, Math.PI + Math.atan(2)],
                           ctm2_3: [40, 80, Math.PI + Math.atan(2)],
                           ctm1:   [40, 80, Math.PI + Math.atan(2)]
  },
  nullMoveBasic : { ctm0:   [0, 0, 0],
                    ctm1_6: [0, 0, 0],
                    ctm1_3: [0, 0, 0],
                    ctm2_3: [0, 0, 0],
                    ctm1:   [0, 0, 0]
  },
  nullMoveRAutoReverse : { ctm0:   [0, 0, Math.PI],
                           ctm1_6: [0, 0, Math.PI],
                           ctm1_3: [0, 0, Math.PI],
                           ctm2_3: [0, 0, Math.PI],
                           ctm1:   [0, 0, Math.PI]
  },
};

var gMotionBundles =
[
  // Bundle to test basic functionality (using default calcMode='paced')
  new TestcaseBundle(gMotionAttr, [
    // Basic paced-mode (default) test, with values/mpath/path
    new AnimMotionTestcase({ "values": "100, 200; 120, 220; 130, 210" },
                           _reusedCTMLists.pacedBasic),
    new AnimMotionTestcase({ "path":  "M100 200 L120 220 L130 210" },
                           _reusedCTMLists.pacedBasic),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210" },
                           _reusedCTMLists.pacedBasic),

    // ..and now with rotate=constant value in degrees
    new AnimMotionTestcase({ "values": "100,200; 120,220; 130, 210",
                             "rotate": "60" },
                           _reusedCTMLists.pacedR60),
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "rotate": "60" },
                           _reusedCTMLists.pacedR60),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210",
                             "rotate": "60" },
                           _reusedCTMLists.pacedR60),

    // ..and now with rotate=constant value in radians
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "rotate": "1.0471975512rad" }, // pi/3
                           _reusedCTMLists.pacedR60),

    // ..and now with rotate=auto
    new AnimMotionTestcase({ "values": "100,200; 120,220; 130, 210",
                             "rotate": "auto" },
                           _reusedCTMLists.pacedRAuto),
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "rotate": "auto" },
                           _reusedCTMLists.pacedRAuto),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210",
                             "rotate": "auto" },
                           _reusedCTMLists.pacedRAuto),

    // ..and now with rotate=auto-reverse
    new AnimMotionTestcase({ "values": "100,200; 120,220; 130, 210",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.pacedRAutoReverse),
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.pacedRAutoReverse),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.pacedRAutoReverse),

  ]),

  // Bundle to test calcMode='discrete'
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "100, 200; 120, 220; 130, 210",
                             "calcMode": "discrete" },
                           _reusedCTMLists.discreteBasic),
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "calcMode": "discrete" },
                           _reusedCTMLists.discreteBasic),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210",
                             "calcMode": "discrete" },
                           _reusedCTMLists.discreteBasic),
    // ..and now with rotate=auto
    new AnimMotionTestcase({ "values": "100, 200; 120, 220; 130, 210",
                             "calcMode": "discrete",
                             "rotate": "auto" },
                           _reusedCTMLists.discreteRAuto),
    new AnimMotionTestcase({ "path": "M100 200 L120 220 L130 210",
                             "calcMode": "discrete",
                             "rotate": "auto" },
                           _reusedCTMLists.discreteRAuto),
    new AnimMotionTestcase({ "mpath": "M100 200 L120 220 L130 210",
                             "calcMode": "discrete",
                             "rotate": "auto" },
                           _reusedCTMLists.discreteRAuto),
  ]),

  // Bundle to test relative units ('em')
  new TestcaseBundle(gMotionAttr, [
    // First with unitless values from->by...
    new AnimMotionTestcase({ "from": "10, 10",
                             "by":   "30, 60" },
                           { ctm0:   [10, 10, 0],
                             ctm1_6: [15, 20, 0],
                             ctm1_3: [20, 30, 0],
                             ctm2_3: [30, 50, 0],
                             ctm1:   [40, 70, 0]
                           }),
    // ... then add 'em' units (with 1em=10px) on half the values
    new AnimMotionTestcase({ "from": "1em, 10",
                             "by":   "30, 6em" },
                           { ctm0:   [10, 10, 0],
                             ctm1_6: [15, 20, 0],
                             ctm1_3: [20, 30, 0],
                             ctm2_3: [30, 50, 0],
                             ctm1:   [40, 70, 0]
                           }),
  ]),

  // Bundle to test a path with just a "move" command and nothing else
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "40, 80" },
                           _reusedCTMLists.justMoveBasic),
    new AnimMotionTestcase({ "path":  "M40 80" },
                           _reusedCTMLists.justMoveBasic),
    new AnimMotionTestcase({ "mpath": "m40 80" },
                           _reusedCTMLists.justMoveBasic),
  ]),
  // ... and now with a fixed rotate-angle
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "40, 80",
                             "rotate": "60" },
                           _reusedCTMLists.justMoveR60),
    new AnimMotionTestcase({ "path":  "M40 80",
                             "rotate": "60" },
                           _reusedCTMLists.justMoveR60),
    new AnimMotionTestcase({ "mpath": "m40 80",
                             "rotate": "60" },
                           _reusedCTMLists.justMoveR60),
  ]),
  // ... and now with 'auto' (should use the move itself as
  // our tangent angle, I think)
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "40, 80",
                             "rotate": "auto" },
                           _reusedCTMLists.justMoveRAuto),
    new AnimMotionTestcase({ "path":  "M40 80",
                             "rotate": "auto" },
                           _reusedCTMLists.justMoveRAuto),
    new AnimMotionTestcase({ "mpath": "m40 80",
                             "rotate": "auto" },
                           _reusedCTMLists.justMoveRAuto),
  ]),
  // ... and now with 'auto-reverse'
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "40, 80",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.justMoveRAutoReverse),
    new AnimMotionTestcase({ "path":  "M40 80",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.justMoveRAutoReverse),
    new AnimMotionTestcase({ "mpath": "m40 80",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.justMoveRAutoReverse),
  ]),
  // ... and now with a null move to make sure 'auto'/'auto-reverse' don't
  // blow up
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "0, 0",
                             "rotate": "auto" },
                           _reusedCTMLists.nullMoveBasic),
  ]),
  new TestcaseBundle(gMotionAttr, [
    new AnimMotionTestcase({ "values": "0, 0",
                             "rotate": "auto-reverse" },
                           _reusedCTMLists.nullMoveRAutoReverse),
  ]),
];

// XXXdholbert Add more tests:
//  - keyPoints/keyTimes
//  - paths with curves
//  - Control path with from/by/to
