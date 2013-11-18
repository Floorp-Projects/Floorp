/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                    ctm2_3: [120, 220, -Math.PI/4],
                    ctm1:   [130, 210, -Math.PI/4]
  },
  pacedRAutoReverse : { ctm0:   [100, 200, 5*Math.PI/4],
                        ctm1_6: [105, 205, 5*Math.PI/4],
                        ctm1_3: [110, 210, 5*Math.PI/4],
                        ctm2_3: [120, 220, 3*Math.PI/4],
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
                    ctm1_3: [120, 220, -Math.PI/4],
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
