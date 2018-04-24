/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* testcase data for simple "from-to" animations of CSS properties */

// NOTE: This js file requires db_smilCSSPropertyList.js

// NOTE: I'm Including 'inherit' and 'currentColor' as interpolatable values.
// According to SVG Mobile 1.2 section 16.2.9, "keywords such as inherit which
// yield a numeric computed value may be included in the values list for an
// interpolated animation".

// Path of test URL (stripping off final slash + filename), for use in
// generating computed value of 'cursor' property
var _testPath = document.URL.substring(0, document.URL.lastIndexOf('/'));

// Lists of testcases for re-use across multiple properties of the same type
var _fromToTestLists = {
  color: [
    new AnimTestcaseFromTo("rgb(100, 100, 100)", "rgb(200, 200, 200)",
                           { midComp: "rgb(150, 150, 150)" }),
    new AnimTestcaseFromTo("#F02000", "#0080A0",
                           { fromComp: "rgb(240, 32, 0)",
                             midComp: "rgb(120, 80, 80)",
                             toComp: "rgb(0, 128, 160)" }),
    new AnimTestcaseFromTo("crimson", "lawngreen",
                           { fromComp: "rgb(220, 20, 60)",
                             midComp: "rgb(172, 136, 30)",
                             toComp: "rgb(124, 252, 0)" }),
    new AnimTestcaseFromTo("currentColor", "rgb(100, 100, 100)",
                           { fromComp: "rgb(50, 50, 50)",
                             midComp: "rgb(75, 75, 75)" }),
    new AnimTestcaseFromTo("rgba(10, 20, 30, 0.2)", "rgba(50, 50, 50, 1)",
                             // (rgb(10, 20, 30) * 0.2 * 0.5 + rgb(50, 50, 50) * 1.0 * 0.5) * (1 / 0.6)
                           { midComp: "rgba(43, 45, 47, 0.6)",
                             toComp:  "rgb(50, 50, 50)"}),
  ],
  colorFromInheritBlack: [
    new AnimTestcaseFromTo("inherit", "rgb(200, 200, 200)",
                           { fromComp: "rgb(0, 0, 0)",
                             midComp: "rgb(100, 100, 100)" }),
  ],
  colorFromInheritWhite: [
    new AnimTestcaseFromTo("inherit", "rgb(205, 205, 205)",
                           { fromComp: "rgb(255, 255, 255)",
                             midComp: "rgb(230, 230, 230)" }),
  ],
  paintServer: [
    new AnimTestcaseFromTo("none", "none"),
    new AnimTestcaseFromTo("none", "blue", { toComp : "rgb(0, 0, 255)" }),
    new AnimTestcaseFromTo("rgb(50, 50, 50)", "none"),
    new AnimTestcaseFromTo("url(#gradA)", "url(#gradB) currentColor",
                           { fromComp: "url(\"" + document.URL +
                                       "#gradA\") rgb(0, 0, 0)",
                             toComp: "url(\"" + document.URL +
                                     "#gradB\") rgb(50, 50, 50)" },
                           "need support for URI-based paints"),
    new AnimTestcaseFromTo("url(#gradA) orange", "url(#gradB)",
                           { fromComp: "url(\"" + document.URL +
                                       "#gradA\") rgb(255, 165, 0)",
                             toComp: "url(\"" + document.URL +
                                     "#gradB\") rgb(0, 0, 0)" },
                           "need support for URI-based paints"),
    new AnimTestcaseFromTo("url(#no_grad)", "url(#gradB)",
                           { fromComp: "url(\"" + document.URL +
                                      "#no_grad\") " + "rgb(0, 0, 0)",
                             toComp: "url(\"" + document.URL +
                                     "#gradB\") rgb(0, 0, 0)" },
                           "need support for URI-based paints"),
    new AnimTestcaseFromTo("url(#no_grad) rgb(1,2,3)", "url(#gradB) blue",
                           { fromComp: "url(\"" + document.URL +
                                       "#no_grad\") " + "rgb(1, 2, 3)",
                             toComp: "url(\"" + document.URL +
                                     "#gradB\") rgb(0, 0, 255)" },
                           "need support for URI-based paints"),
  ],
  lengthNoUnits: [
    new AnimTestcaseFromTo("0",  "20", { fromComp:  "0px",
                                         midComp:  "10px",
                                         toComp:   "20px"}),
    new AnimTestcaseFromTo("50",  "0", { fromComp: "50px",
                                         midComp:  "25px",
                                         toComp:    "0px"}),
    new AnimTestcaseFromTo("30", "80", { fromComp: "30px",
                                         midComp:  "55px",
                                         toComp:   "80px"}),
  ],
  lengthPx: [
    new AnimTestcaseFromTo("0px", "12px", { fromComp: "0px",
                                            midComp:  "6px",
                                            toComp:   "12px"}),
    new AnimTestcaseFromTo("16px", "0px", { fromComp: "16px",
                                            midComp:  "8px",
                                            toComp:   "0px"}),
    new AnimTestcaseFromTo("10px", "20px", { fromComp: "10px",
                                             midComp:  "15px",
                                             toComp:   "20px"}),
    new AnimTestcaseFromTo("41px", "1px", { fromComp: "41px",
                                            midComp:  "21px",
                                            toComp:   "1px"}),
  ],
  lengthPctSVG: [
    new AnimTestcaseFromTo("20.5%", "0.5%", { midComp: "10.5%" }),
  ],
  lengthPxPctSVG: [
    new AnimTestcaseFromTo("10px", "10%", { midComp: "15px"},
                           "need support for interpolating between " +
                           "px and percent values"),
  ],
  lengthPxNoUnitsSVG: [
    new AnimTestcaseFromTo("10", "20px", { fromComp: "10px",
                                           midComp:  "15px",
                                           toComp:   "20px"}),
    new AnimTestcaseFromTo("10px", "20", { fromComp: "10px",
                                           midComp:  "15px",
                                           toComp:   "20px"}),
  ],
  opacity: [
    new AnimTestcaseFromTo("1", "0", { midComp: "0.5" }),
    new AnimTestcaseFromTo("0.2", "0.12", { midComp: "0.16" }),
    new AnimTestcaseFromTo("0.5", "0.7", { midComp: "0.6" }),
    new AnimTestcaseFromTo("0.5", "inherit",
                           { midComp: "0.75", toComp: "1" }),
    // Make sure we don't clamp out-of-range values before interpolation
    new AnimTestcaseFromTo("0.2", "1.2",
                           { midComp: "0.7", toComp: "1" },
                           "opacities with abs val >1 get clamped too early"),
    new AnimTestcaseFromTo("-0.2", "0.6",
                           { fromComp: "0", midComp: "0.2" }),
    new AnimTestcaseFromTo("-1.2", "1.6",
                           { fromComp: "0", midComp: "0.2", toComp: "1" },
                           "opacities with abs val >1 get clamped too early"),
    new AnimTestcaseFromTo("-0.6", "1.4",
                           { fromComp: "0", midComp: "0.4", toComp: "1" },
                           "opacities with abs val >1 get clamped too early"),
  ],
  URIsAndNone: [
    new AnimTestcaseFromTo("url(#idA)", "url(#idB)",
                           { fromComp: "url(\"#idA\")",
                             toComp: "url(\"#idB\")"}),
    new AnimTestcaseFromTo("none", "url(#idB)",
                           { toComp: "url(\"#idB\")"}),
    new AnimTestcaseFromTo("url(#idB)", "inherit",
                           { fromComp: "url(\"#idB\")",
                             toComp: "none"}),
  ],
};

// List of attribute/testcase-list bundles to be tested
var gFromToBundles = [
  new TestcaseBundle(gPropList.clip, [
    new AnimTestcaseFromTo("rect(1px, 2px, 3px, 4px)",
                           "rect(11px, 22px, 33px, 44px)",
                           { midComp: "rect(6px, 12px, 18px, 24px)" }),
    new AnimTestcaseFromTo("rect(1px, auto, 3px, 4px)",
                           "rect(11px, auto, 33px, 44px)",
                           { midComp: "rect(6px, auto, 18px, 24px)" }),
    new AnimTestcaseFromTo("auto", "auto"),
    new AnimTestcaseFromTo("rect(auto, auto, auto, auto)",
                           "rect(auto, auto, auto, auto)"),
    // Interpolation not supported in these next cases (with auto --> px-value)
    new AnimTestcaseFromTo("rect(1px, auto, 3px, auto)",
                           "rect(11px, auto, 33px, 44px)"),
    new AnimTestcaseFromTo("rect(1px, 2px, 3px, 4px)",
                           "rect(11px, auto, 33px, 44px)"),
    new AnimTestcaseFromTo("rect(1px, 2px, 3px, 4px)", "auto"),
    new AnimTestcaseFromTo("auto", "rect(1px, 2px, 3px, 4px)"),
  ]),
  new TestcaseBundle(gPropList.clip_path, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.clip_rule, [
    new AnimTestcaseFromTo("nonzero", "evenodd"),
    new AnimTestcaseFromTo("evenodd", "inherit", { toComp: "nonzero" }),
  ]),
  new TestcaseBundle(gPropList.color,
                     [].concat(_fromToTestLists.color, [
    // Note: inherited value is rgb(50, 50, 50) (set on <svg>)
    new AnimTestcaseFromTo("inherit", "rgb(200, 200, 200)",
                           { fromComp: "rgb(50, 50, 50)",
                             midComp: "rgb(125, 125, 125)" }),
  ])),
  new TestcaseBundle(gPropList.color_interpolation, [
    new AnimTestcaseFromTo("sRGB", "auto", { fromComp: "srgb" }),
    new AnimTestcaseFromTo("inherit", "linearRGB",
                         { fromComp: "srgb", toComp: "linearrgb" }),
  ]),
  new TestcaseBundle(gPropList.color_interpolation_filters, [
    new AnimTestcaseFromTo("sRGB", "auto", { fromComp: "srgb" }),
    new AnimTestcaseFromTo("auto", "inherit",
                         { toComp: "linearrgb" }),
  ]),
  new TestcaseBundle(gPropList.cursor, [
    new AnimTestcaseFromTo("crosshair", "move"),
    new AnimTestcaseFromTo("url('a.cur'), url('b.cur'), nw-resize", "sw-resize",
                           { fromComp: "url(\"" + _testPath + "/a.cur\"), " +
                                       "url(\"" + _testPath + "/b.cur\"), " +
                                       "nw-resize"}),
  ]),
  new TestcaseBundle(gPropList.direction, [
    new AnimTestcaseFromTo("ltr", "rtl"),
    new AnimTestcaseFromTo("rtl", "inherit"),
  ]),
  new TestcaseBundle(gPropList.display, [
    // I'm not testing the "inherit" value for "display", because part of
    // my test runs with "display: none" on everything, and so the
    // inherited value isn't always the same.  (i.e. the computed value
    // of 'inherit' will be different in different tests)
    new AnimTestcaseFromTo("block", "table-cell"),
    new AnimTestcaseFromTo("inline", "inline-table"),
    new AnimTestcaseFromTo("table-row", "none"),
  ]),
  new TestcaseBundle(gPropList.dominant_baseline, [
    new AnimTestcaseFromTo("use-script", "no-change"),
    new AnimTestcaseFromTo("reset-size", "ideographic"),
    new AnimTestcaseFromTo("alphabetic", "hanging"),
    new AnimTestcaseFromTo("mathematical", "central"),
    new AnimTestcaseFromTo("middle", "text-after-edge"),
    new AnimTestcaseFromTo("text-before-edge", "auto"),
    new AnimTestcaseFromTo("use-script", "inherit", { toComp: "auto" } ),
  ]),
  // NOTE: Mozilla doesn't currently support "enable-background", but I'm
  // testing it here in case we ever add support for it, because it's
  // explicitly not animatable in the SVG spec.
  new TestcaseBundle(gPropList.enable_background, [
    new AnimTestcaseFromTo("new", "accumulate"),
  ]),
  new TestcaseBundle(gPropList.fill,
                     [].concat(_fromToTestLists.color,
                               _fromToTestLists.paintServer,
                               _fromToTestLists.colorFromInheritBlack)),
  new TestcaseBundle(gPropList.fill_opacity, _fromToTestLists.opacity),
  new TestcaseBundle(gPropList.fill_rule, [
    new AnimTestcaseFromTo("nonzero", "evenodd"),
    new AnimTestcaseFromTo("evenodd", "inherit", { toComp: "nonzero" }),
  ]),
  new TestcaseBundle(gPropList.filter, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.flood_color,
                     [].concat(_fromToTestLists.color,
                               _fromToTestLists.colorFromInheritBlack)),
  new TestcaseBundle(gPropList.flood_opacity, _fromToTestLists.opacity),
  new TestcaseBundle(gPropList.font, [
    // NOTE: 'line-height' is hard-wired at 10px in test_smilCSSFromTo.xhtml
    // because if it's not explicitly set, its value varies across platforms.
    // NOTE: System font values can't be tested here, because their computed
    // values vary from platform to platform.  However, they are tested
    // visually, in the reftest "anim-css-font-1.svg"
    new AnimTestcaseFromTo("10px serif", "30px serif",
                           { fromComp: "normal normal 400 10px / 10px serif",
                             toComp: "normal normal 400 30px / 10px serif"}),
    new AnimTestcaseFromTo("10px serif", "30px sans-serif",
                         { fromComp: "normal normal 400 10px / 10px serif",
                          toComp: "normal normal 400 30px / 10px sans-serif"}),
    new AnimTestcaseFromTo("1px / 90px cursive", "100px monospace",
                         { fromComp: "normal normal 400 1px / 10px cursive",
                           toComp: "normal normal 400 100px / 10px monospace"}),
    new AnimTestcaseFromTo("italic small-caps 200 1px cursive",
                           "100px monospace",
                        { fromComp: "italic small-caps 200 1px / 10px cursive",
                          toComp: "normal normal 400 100px / 10px monospace"}),
    new AnimTestcaseFromTo("oblique normal 200 30px / 10px cursive",
                           "normal small-caps 800 40px / 10px serif"),
  ]),
  new TestcaseBundle(gPropList.font_family, [
    new AnimTestcaseFromTo("serif", "sans-serif"),
    new AnimTestcaseFromTo("cursive", "monospace"),
  ]),
  new TestcaseBundle(gPropList.font_size,
                     [].concat(_fromToTestLists.lengthNoUnits,
                               _fromToTestLists.lengthPx, [
    new AnimTestcaseFromTo("10px", "40%", { midComp: "15px", toComp: "20px" }),
    new AnimTestcaseFromTo("160%", "80%",
                           { fromComp: "80px",
                             midComp: "60px",
                             toComp: "40px"}),
  ])),
  new TestcaseBundle(gPropList.font_size_adjust, [
    new AnimTestcaseFromTo("0.9", "0.1", { midComp: "0.5" }),
    new AnimTestcaseFromTo("0.5", "0.6", { midComp: "0.55" }),
    new AnimTestcaseFromTo("none", "0.4"),
  ]),
  new TestcaseBundle(gPropList.font_stretch, [
    new AnimTestcaseFromTo("normal", "wider", {},
                           "need support for animating between " +
                           "relative 'font-stretch' values"),
    new AnimTestcaseFromTo("narrower", "ultra-condensed", {},
                           "need support for animating between " +
                           "relative 'font-stretch' values"),
    new AnimTestcaseFromTo("ultra-condensed", "condensed",
                           { fromComp: "50%",
                             midComp: "62.5%",
                             toComp: "75%"}),
    new AnimTestcaseFromTo("semi-condensed", "semi-expanded",
                           { fromComp: "87.5%",
                             midComp: "100%",
                             toComp: "112.5%"}),
    new AnimTestcaseFromTo("expanded", "ultra-expanded",
                           { fromComp: "125%",
                             midComp: "162.5%",
                             toComp: "200%"}),
    new AnimTestcaseFromTo("ultra-expanded", "inherit",
                           { fromComp: "200%",
                             midComp: "150%",
                             toComp: "100%"}),
  ]),
  new TestcaseBundle(gPropList.font_style, [
    new AnimTestcaseFromTo("italic", "inherit", { toComp: "normal" }),
    new AnimTestcaseFromTo("normal", "italic"),
    new AnimTestcaseFromTo("italic", "oblique"),
    new AnimTestcaseFromTo("oblique", "normal"),
  ]),
  new TestcaseBundle(gPropList.font_variant, [
    new AnimTestcaseFromTo("inherit", "small-caps", { fromComp: "normal" }),
    new AnimTestcaseFromTo("small-caps", "normal"),
  ]),
  new TestcaseBundle(gPropList.font_weight, [
    new AnimTestcaseFromTo("100", "900", { midComp: "500" }),
    new AnimTestcaseFromTo("700", "100", { midComp: "400" }),
    new AnimTestcaseFromTo("inherit", "200",
                           { fromComp: "400", midComp: "300" }),
    new AnimTestcaseFromTo("normal", "bold",
                           { fromComp: "400", midComp: "550", toComp: "700" }),
    new AnimTestcaseFromTo("lighter", "bolder", {},
                           "need support for animating between " +
                           "relative 'font-weight' values"),
  ]),
  // NOTE: Mozilla doesn't currently support "glyph-orientation-horizontal" or
  // "glyph-orientation-vertical", but I'm testing them here in case we ever
  // add support for them, because they're explicitly not animatable in the SVG
  // spec.
  new TestcaseBundle(gPropList.glyph_orientation_horizontal,
                     [ new AnimTestcaseFromTo("45deg", "60deg") ]),
  new TestcaseBundle(gPropList.glyph_orientation_vertical,
                     [ new AnimTestcaseFromTo("45deg", "60deg") ]),
  new TestcaseBundle(gPropList.image_rendering, [
    new AnimTestcaseFromTo("auto", "optimizeQuality",
                           { toComp: "optimizequality" }),
    new AnimTestcaseFromTo("optimizeQuality", "optimizeSpeed",
                           { fromComp: "optimizequality",
                             toComp: "optimizespeed" }),
  ]),
  new TestcaseBundle(gPropList.letter_spacing,
                     [].concat(_fromToTestLists.lengthNoUnits,
                               _fromToTestLists.lengthPx,
                               _fromToTestLists.lengthPxPctSVG)),
  new TestcaseBundle(gPropList.letter_spacing,
                     _fromToTestLists.lengthPctSVG,
                     "pct->pct animations don't currently work for " +
                     "*-spacing properties"),
  new TestcaseBundle(gPropList.lighting_color,
                     [].concat(_fromToTestLists.color,
                               _fromToTestLists.colorFromInheritWhite)),
  new TestcaseBundle(gPropList.marker, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_end, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_mid, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.marker_start, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.mask, _fromToTestLists.URIsAndNone),
  new TestcaseBundle(gPropList.opacity, _fromToTestLists.opacity),
  new TestcaseBundle(gPropList.overflow, [
    new AnimTestcaseFromTo("auto", "visible"),
    new AnimTestcaseFromTo("inherit", "visible", { fromComp: "hidden" }),
    new AnimTestcaseFromTo("scroll", "auto"),
  ]),
  new TestcaseBundle(gPropList.pointer_events, [
    new AnimTestcaseFromTo("visibleFill", "stroke",
                           { fromComp: "visiblefill" }),
    new AnimTestcaseFromTo("none", "visibleStroke",
                           { toComp: "visiblestroke" }),
  ]),
  new TestcaseBundle(gPropList.shape_rendering, [
    new AnimTestcaseFromTo("auto", "optimizeSpeed",
                           { toComp: "optimizespeed" }),
    new AnimTestcaseFromTo("crispEdges", "geometricPrecision",
                           { fromComp: "crispedges",
                             toComp: "geometricprecision" }),
  ]),
  new TestcaseBundle(gPropList.stop_color,
                        [].concat(_fromToTestLists.color,
                                  _fromToTestLists.colorFromInheritBlack)),
  new TestcaseBundle(gPropList.stop_opacity, _fromToTestLists.opacity),
  new TestcaseBundle(gPropList.stroke,
                     [].concat(_fromToTestLists.color,
                               _fromToTestLists.paintServer, [
     // Note: inherited value is "none" (the default for "stroke" property)
     new AnimTestcaseFromTo("inherit", "rgb(200, 200, 200)",
                            { fromComp: "none"})])),
  new TestcaseBundle(gPropList.stroke_dasharray,
                     [].concat(_fromToTestLists.lengthPctSVG,
                               [
    new AnimTestcaseFromTo("inherit", "20", { fromComp: "none"}),
    new AnimTestcaseFromTo("1", "none"),
    new AnimTestcaseFromTo("10", "20", { midComp: "15"}),
    new AnimTestcaseFromTo("1", "2, 3", { fromComp: "1, 1",
                                          midComp: "1.5, 2"}),
    new AnimTestcaseFromTo("2, 8", "6", { midComp: "4, 7"}),
    new AnimTestcaseFromTo("1, 3", "1, 3, 5, 7, 9",
                           { fromComp: "1, 3, 1, 3, 1, 3, 1, 3, 1, 3",
                             midComp:  "1, 3, 3, 5, 5, 2, 2, 4, 4, 6"}),
  ])),
  new TestcaseBundle(gPropList.stroke_dashoffset,
                     [].concat(_fromToTestLists.lengthNoUnits,
                               _fromToTestLists.lengthPx,
                               _fromToTestLists.lengthPxPctSVG,
                               _fromToTestLists.lengthPctSVG,
                               _fromToTestLists.lengthPxNoUnitsSVG)),
  new TestcaseBundle(gPropList.stroke_linecap, [
    new AnimTestcaseFromTo("butt", "round"),
    new AnimTestcaseFromTo("round", "square"),
  ]),
  new TestcaseBundle(gPropList.stroke_linejoin, [
    new AnimTestcaseFromTo("miter", "round"),
    new AnimTestcaseFromTo("round", "bevel"),
  ]),
  new TestcaseBundle(gPropList.stroke_miterlimit, [
    new AnimTestcaseFromTo("1", "2", { midComp: "1.5" }),
    new AnimTestcaseFromTo("20.1", "10.1", { midComp: "15.1" }),
  ]),
  new TestcaseBundle(gPropList.stroke_opacity, _fromToTestLists.opacity),
  new TestcaseBundle(gPropList.stroke_width,
                     [].concat(_fromToTestLists.lengthNoUnits,
                               _fromToTestLists.lengthPx,
                               _fromToTestLists.lengthPxPctSVG,
                               _fromToTestLists.lengthPctSVG,
                               _fromToTestLists.lengthPxNoUnitsSVG, [
    new AnimTestcaseFromTo("inherit", "7px",
                           { fromComp: "1px", midComp: "4px", toComp: "7px" }),
  ])),
  new TestcaseBundle(gPropList.text_anchor, [
    new AnimTestcaseFromTo("start", "middle"),
    new AnimTestcaseFromTo("middle", "end"),
  ]),
  new TestcaseBundle(gPropList.text_decoration, [
    new AnimTestcaseFromTo("none", "underline"),
    new AnimTestcaseFromTo("overline", "line-through"),
    new AnimTestcaseFromTo("blink", "underline"),
  ]),
  new TestcaseBundle(gPropList.text_rendering, [
    new AnimTestcaseFromTo("auto", "optimizeSpeed",
                           { toComp: "optimizespeed" }),
    new AnimTestcaseFromTo("optimizeSpeed", "geometricPrecision",
                           { fromComp: "optimizespeed",
                             toComp: "geometricprecision" }),
    new AnimTestcaseFromTo("geometricPrecision", "optimizeLegibility",
                           { fromComp: "geometricprecision",
                             toComp: "optimizelegibility" }),
  ]),
  new TestcaseBundle(gPropList.unicode_bidi, [
    new AnimTestcaseFromTo("embed", "bidi-override"),
  ]),
  new TestcaseBundle(gPropList.vector_effect, [
    new AnimTestcaseFromTo("none", "non-scaling-stroke"),
  ]),
  new TestcaseBundle(gPropList.visibility, [
    new AnimTestcaseFromTo("visible", "hidden"),
    new AnimTestcaseFromTo("hidden", "collapse"),
  ]),
  new TestcaseBundle(gPropList.word_spacing,
                     [].concat(_fromToTestLists.lengthNoUnits,
                               _fromToTestLists.lengthPx,
                               _fromToTestLists.lengthPxPctSVG)),
  new TestcaseBundle(gPropList.word_spacing,
                     _fromToTestLists.lengthPctSVG,
                     "pct->pct animations don't currently work for " +
                     "*-spacing properties"),
  // NOTE: Mozilla doesn't currently support "writing-mode", but I'm
  // testing it here in case we ever add support for it, because it's
  // explicitly not animatable in the SVG spec.
  new TestcaseBundle(gPropList.writing_mode, [
    new AnimTestcaseFromTo("lr", "rl"),
  ]),
];
