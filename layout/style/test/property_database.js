/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* vim: set shiftwidth=4 tabstop=4 autoindent cindent noexpandtab: */
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
 * The Original Code is property_database.js.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *   Mats Palmgren <matspal@gmail.com>
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

// True longhand properties.
const CSS_TYPE_LONGHAND = 0;

// True shorthand properties.
const CSS_TYPE_TRUE_SHORTHAND = 1;

// Properties that we handle as shorthands but were longhands either in
// the current spec or earlier versions of the spec.
const CSS_TYPE_SHORTHAND_AND_LONGHAND = 2;

// Each property has the following fields:
//	 domProp: The name of the relevant member of nsIDOM[NS]CSS2Properties
//	 inherited: Whether the property is inherited by default (stated as 
//	   yes or no in the property header in all CSS specs)
//	 type: see above
//	 get_computed: if present, the property's computed value shows up on
//	   another property, and this is a function used to get it
//	 initial_values: Values whose computed value should be the same as the
//	   computed value for the property's initial value.
//	 other_values: Values whose computed value should be different from the
//	   computed value for the property's initial value.
//	 XXX Should have a third field for values whose computed value may or
//	   may not be the same as for the property's initial value.
//	 invalid_values: Things that are not values for the property and
//	   should be rejected.

// Helper functions used to construct gCSSProperties.

function initial_font_family_is_sans_serif()
{
	// The initial value of 'font-family' might be 'serif' or
	// 'sans-serif'.
	var div = document.createElement("div");
	div.setAttribute("style", "font: -moz-initial");
	return getComputedStyle(div, "").fontFamily == "sans-serif";
}
var gInitialFontFamilyIsSansSerif = initial_font_family_is_sans_serif();

var gCSSProperties = {
	"-moz-animation": {
		domProp: "MozAnimation",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-animation-name", "-moz-animation-duration", "-moz-animation-timing-function", "-moz-animation-delay", "-moz-animation-direction", "-moz-animation-fill-mode", "-moz-animation-iteration-count" ],
		initial_values: [ "none none 0s 0s ease normal 1.0", "none", "0s", "ease", "normal", "1.0" ],
		other_values: [ "bounce 1s linear 2s", "bounce 1s 2s linear", "bounce linear 1s 2s", "linear bounce 1s 2s", "linear 1s bounce 2s", "linear 1s 2s bounce", "1s bounce linear 2s", "1s bounce 2s linear", "1s 2s bounce linear", "1s linear bounce 2s", "1s linear 2s bounce", "1s 2s linear bounce", "bounce linear 1s", "bounce 1s linear", "linear bounce 1s", "linear 1s bounce", "1s bounce linear", "1s linear bounce", "1s 2s bounce", "1s bounce 2s", "bounce 1s 2s", "1s 2s linear", "1s linear 2s", "linear 1s 2s", "bounce 1s", "1s bounce", "linear 1s", "1s linear", "1s 2s", "2s 1s", "bounce", "linear", "1s", "height", "2s", "ease-in-out", "2s ease-in", "opacity linear", "ease-out 2s", "2s color, 1s bounce, 500ms height linear, 1s opacity 4s cubic-bezier(0.0, 0.1, 1.0, 1.0)", "1s \\32bounce linear 2s", "1s -bounce linear 2s", "1s -\\32bounce linear 2s", "1s \\32 0bounce linear 2s", "1s -\\32 0bounce linear 2s", "1s \\2bounce linear 2s", "1s -\\2bounce linear 2s", "2s, 1s bounce", "1s bounce, 2s", "2s all, 1s bounce", "1s bounce, 2s all", "1s bounce, 2s none", "2s none, 1s bounce", "2s bounce, 1s all", "2s all, 1s bounce" ],
		invalid_values: [  "2s inherit", "inherit 2s", "2s bounce, 1s inherit", "2s inherit, 1s bounce", "2s initial" ]
	},
	"-moz-animation-delay": {
		domProp: "MozAnimationDelay",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0s", "0ms" ],
		other_values: [ "1s", "250ms", "-100ms", "-1s", "1s, 250ms, 2.3s"],
		invalid_values: [ "0", "0px" ]
	},
	"-moz-animation-direction": {
		domProp: "MozAnimationDirection",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "alternate", "normal, alternate", "alternate, normal", "normal, normal", "normal, normal, normal" ],
		invalid_values: [ "normal normal", "inherit, normal" ]
	},
	"-moz-animation-duration": {
		domProp: "MozAnimationDuration",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0s", "0ms" ],
		other_values: [ "1s", "250ms", "-1ms", "-2s", "1s, 250ms, 2.3s"],
		invalid_values: [ "0", "0px" ]
	},
	"-moz-animation-fill-mode": {
		domProp: "MozAnimationFillMode",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "forwards", "backwards", "both", "none, none", "forwards, backwards", "forwards, none", "none, both" ],
		invalid_values: [ "all"]
	},
	"-moz-animation-iteration-count": {
		domProp: "MozAnimationIterationCount",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1" ],
		other_values: [ "infinite", "0", "0.5", "7.75", "-0.0", "1, 2, 3", "infinite, 2", "1, infinite" ],
		// negatives forbidden per
		// http://lists.w3.org/Archives/Public/www-style/2011Mar/0355.html
		invalid_values: [ "none", "-1", "-0.5", "-1, infinite", "infinite, -3" ]
	},
	"-moz-animation-name": {
		domProp: "MozAnimationName",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "all", "ball", "mall", "color", "bounce, bubble, opacity", "foobar", "auto", "\\32bounce", "-bounce", "-\\32bounce", "\\32 0bounce", "-\\32 0bounce", "\\2bounce", "-\\2bounce" ],
		invalid_values: [ "bounce, initial", "initial, bounce", "bounce, inherit", "inherit, bounce" ]
	},
	"-moz-animation-play-state": {
		domProp: "MozAnimationPlayState",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "running" ],
		other_values: [ "paused", "running, running", "paused, running", "paused, paused", "running, paused", "paused, running, running, running, paused, running" ],
		invalid_values: [ "0" ]
	},
	"-moz-animation-timing-function": {
		domProp: "MozAnimationTimingFunction",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "ease", "cubic-bezier(0.25, 0.1, 0.25, 1.0)" ],
		other_values: [ "linear", "ease-in", "ease-out", "ease-in-out", "linear, ease-in, cubic-bezier(0.1, 0.2, 0.8, 0.9)", "cubic-bezier(0.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.25, 1.5, 0.75, -0.5)", "step-start", "step-end", "steps(1)", "steps(2, start)", "steps(386)", "steps(3, end)" ],
		invalid_values: [ "none", "auto", "cubic-bezier(0.25, 0.1, 0.25)", "cubic-bezier(0.25, 0.1, 0.25, 0.25, 1.0)", "cubic-bezier(-0.5, 0.5, 0.5, 0.5)", "cubic-bezier(1.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.5, 0.5, -0.5, 0.5)", "cubic-bezier(0.5, 0.5, 1.5, 0.5)", "steps(2, step-end)", "steps(0)", "steps(-2)", "steps(0, step-end, 1)" ]
	},
	"-moz-appearance": {
		domProp: "MozAppearance",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "radio", "menulist" ],
		invalid_values: []
	},
	"-moz-background-inline-policy": {
		domProp: "MozBackgroundInlinePolicy",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "continuous" ],
		other_values: ["bounding-box", "each-box" ],
		invalid_values: []
	},
	"-moz-binding": {
		domProp: "MozBinding",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(foo.xml)" ],
		invalid_values: []
	},
	"-moz-border-bottom-colors": {
		domProp: "MozBorderBottomColors",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
		invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red" ]
	},
	"-moz-border-end": {
		domProp: "MozBorderEnd",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-border-end-color", "-moz-border-end-style", "-moz-border-end-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"-moz-border-end-color": {
		domProp: "MozBorderEndColor",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"-moz-border-end-style": {
		domProp: "MozBorderEndStyle",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"-moz-border-end-width": {
		domProp: "MozBorderEndWidth",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		prerequisites: { "-moz-border-end-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"-moz-border-image": {
		domProp: "MozBorderImage",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url('border.png') 27 27 27 27",
						"url('border.png') 27",
						"url('border.png') 27 27 27 27 repeat",
						"url('border.png') 27 27 27 27 / 1em",
						"url('border.png') 27 27 27 27 / 1em 1em 1em 1em repeat",
						"url('border.png') 27 27 27 27 / 1em 1em 1em 1em stretch round" ],
		invalid_values: [ "url('border.png')",
						  "url('border.png') 27 27 27 27 27",
						  "url('border.png') 27 27 27 27 / 1em 1em 1em 1em 1em",
						  "url('border.png') / repeat",
						  "url('border.png') 27 27 27 27 /" ]
	},
	"-moz-border-left-colors": {
		domProp: "MozBorderLeftColors",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
		invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red" ]
	},
	"border-radius": {
		domProp: "borderRadius",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		subproperties: [ "border-bottom-left-radius", "border-bottom-right-radius", "border-top-left-radius", "border-top-right-radius" ],
		initial_values: [ "0", "0px", "0%", "0px 0 0 0px", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px) -moz-calc(0pt) -moz-calc(0%) -moz-calc(0em)" ],
		other_values: [ "3%", "1px", "2em", "3em 2px", "2pt 3% 4em", "2px 2px 2px 2px", // circular
						"3% / 2%", "1px / 4px", "2em / 1em", "3em 2px / 2px 3em", "2pt 3% 4em / 4pt 1% 5em", "2px 2px 2px 2px / 4px 4px 4px 4px", "1pt / 2pt 3pt", "4pt 5pt / 3pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
			"2px 2px -moz-calc(2px + 1%) 2px",
			"1px 2px 2px 2px / 2px 2px -moz-calc(2px + 1%) 2px",
					  ],
		invalid_values: [ "2px -2px", "inherit 2px", "inherit / 2px", "2px inherit", "2px / inherit", "2px 2px 2px 2px 2px", "1px / 2px 2px 2px 2px 2px" ]
	},
	"border-bottom-left-radius": {
		domProp: "borderBottomLeftRadius",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"border-bottom-right-radius": {
		domProp: "borderBottomRightRadius",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"border-top-left-radius": {
		domProp: "borderTopLeftRadius",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"border-top-right-radius": {
		domProp: "borderTopRightRadius",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"-moz-border-right-colors": {
		domProp: "MozBorderRightColors",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
		invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red" ]
	},
	"-moz-border-start": {
		domProp: "MozBorderStart",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-border-start-color", "-moz-border-start-style", "-moz-border-start-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"-moz-border-start-color": {
		domProp: "MozBorderStartColor",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"-moz-border-start-style": {
		domProp: "MozBorderStartStyle",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"-moz-border-start-width": {
		domProp: "MozBorderStartWidth",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		prerequisites: { "-moz-border-start-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"-moz-border-top-colors": {
		domProp: "MozBorderTopColors",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red green", "red #fc3", "#ff00cc", "currentColor", "blue currentColor orange currentColor" ],
		invalid_values: [ "red none", "red inherit", "red, green", "none red", "inherit red" ]
	},
	"-moz-box-align": {
		domProp: "MozBoxAlign",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "stretch" ],
		other_values: [ "start", "center", "baseline", "end" ],
		invalid_values: []
	},
	"-moz-box-direction": {
		domProp: "MozBoxDirection",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "reverse" ],
		invalid_values: []
	},
	"-moz-box-flex": {
		domProp: "MozBoxFlex",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0.0", "-0.0" ],
		other_values: [ "1", "100", "0.1" ],
		invalid_values: [ "10px", "-1" ]
	},
	"-moz-box-ordinal-group": {
		domProp: "MozBoxOrdinalGroup",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1" ],
		other_values: [ "2", "100", "0" ],
		invalid_values: [ "1.0", "-1", "-1000" ]
	},
	"-moz-box-orient": {
		domProp: "MozBoxOrient",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "horizontal", "inline-axis" ],
		other_values: [ "vertical", "block-axis" ],
		invalid_values: []
	},
	"-moz-box-pack": {
		domProp: "MozBoxPack",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "start" ],
		other_values: [ "center", "end", "justify" ],
		invalid_values: []
	},
	"-moz-box-sizing": {
		domProp: "MozBoxSizing",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "content-box" ],
		other_values: [ "border-box", "padding-box" ],
		invalid_values: [ "margin-box", "content", "padding", "border", "margin" ]
	},
	"-moz-columns": {
		domProp: "MozColumns",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-column-count", "-moz-column-width" ],
		initial_values: [ "auto", "auto auto" ],
		other_values: [ "3", "20px", "2 10px", "10px 2", "2 auto", "auto 2", "auto 50px", "50px auto" ],
		invalid_values: [ "5%", "-1px", "-1", "3 5", "10px 4px", "10 2px 5in", "30px -1",
		                  "auto 3 5px", "5 auto 20px", "auto auto auto" ]
	},
	"-moz-column-count": {
		domProp: "MozColumnCount",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "1", "17" ],
		// negative and zero invalid per editor's draft
		invalid_values: [ "-1", "0", "3px" ]
	},
	"-moz-column-gap": {
		domProp: "MozColumnGap",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal", "1em", "-moz-calc(-2em + 3em)" ],
		other_values: [ "2px", "4em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0px)",
			"-moz-calc(0pt)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "3%", "-1px" ]
	},
	"-moz-column-rule": {
		domProp: "MozColumnRule",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		prerequisites: { "color": "green" },
		subproperties: [ "-moz-column-rule-width", "-moz-column-rule-style", "-moz-column-rule-color" ],
		initial_values: [ "medium none currentColor" ],
		other_values: [ "2px blue solid", "red dotted 1px", "ridge 4px orange" ],
		invalid_values: [ "2px 3px 4px red", "dotted dashed", "5px dashed green 3px" ]
	},
	"-moz-column-rule-width": {
		domProp: "MozColumnRuleWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "-moz-column-rule-style": "solid" },
		initial_values: [
			"medium",
			"3px",
			"-moz-calc(3px)",
			"-moz-calc(5em + 3px - 5em)"
		],
		other_values: [ "thin", "15px",
			/* valid calc() values */
			"-moz-calc(-2px)",
			"-moz-calc(2px)",
			"-moz-calc(3em)",
			"-moz-calc(3em + 2px)",
			"-moz-calc( 3em + 2px)",
			"-moz-calc(3em + 2px )",
			"-moz-calc( 3em + 2px )",
			"-moz-calc(3*25px)",
			"-moz-calc(3 *25px)",
			"-moz-calc(3 * 25px)",
			"-moz-calc(3* 25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(25px *3)",
			"-moz-calc(25px* 3)",
			"-moz-calc(25px * 3)",
			"-moz-calc(25px * 3 / 4)",
			"-moz-calc((25px * 3) / 4)",
			"-moz-calc(25px * (3 / 4))",
			"-moz-calc(3 * 25px / 4)",
			"-moz-calc((3 * 25px) / 4)",
			"-moz-calc(3 * (25px / 4))",
			"-moz-calc(3em + 25px * 3 / 4)",
			"-moz-calc(3em + (25px * 3) / 4)",
			"-moz-calc(3em + 25px * (3 / 4))",
			"-moz-calc(25px * 3 / 4 + 3em)",
			"-moz-calc((25px * 3) / 4 + 3em)",
			"-moz-calc(25px * (3 / 4) + 3em)",
			"-moz-calc(3em + (25px * 3 / 4))",
			"-moz-calc(3em + ((25px * 3) / 4))",
			"-moz-calc(3em + (25px * (3 / 4)))",
			"-moz-calc((25px * 3 / 4) + 3em)",
			"-moz-calc(((25px * 3) / 4) + 3em)",
			"-moz-calc((25px * (3 / 4)) + 3em)",
			"-moz-calc(3*25px + 1in)",
			"-moz-calc(1in - 3em + 2px)",
			"-moz-calc(1in - (3em + 2px))",
			"-moz-calc((1in - 3em) + 2px)",
			"-moz-calc(50px/2)",
			"-moz-calc(50px/(2 - 1))",
			"-moz-calc(-3px)",
			/* numeric reduction cases */
			"-moz-calc(5 * 3 * 2em)",
			"-moz-calc(2em * 5 * 3)",
			"-moz-calc((5 * 3) * 2em)",
			"-moz-calc(2em * (5 * 3))",
			"-moz-calc((5 + 3) * 2em)",
			"-moz-calc(2em * (5 + 3))",
			"-moz-calc(2em / (5 + 3))",
			"-moz-calc(2em * (5*2 + 3))",
			"-moz-calc(2em * ((5*2) + 3))",
			"-moz-calc(2em * (5*(2 + 3)))",

			"-moz-calc((5 + 7) * 3em)",
			"-moz-calc((5em + 3em) - 2em)",
			"-moz-calc((5em - 3em) + 2em)",
			"-moz-calc(2em - (5em - 3em))",
			"-moz-calc(2em + (5em - 3em))",
			"-moz-calc(2em - (5em + 3em))",
			"-moz-calc(2em + (5em + 3em))",
			"-moz-calc(2em + 5em - 3em)",
			"-moz-calc(2em - 5em - 3em)",
			"-moz-calc(2em + 5em + 3em)",
			"-moz-calc(2em - 5em + 3em)",

			"-moz-calc(2em / 4 * 3)",
			"-moz-calc(2em * 4 / 3)",
			"-moz-calc(2em * 4 * 3)",
			"-moz-calc(2em / 4 / 3)",
			"-moz-calc(4 * 2em / 3)",
			"-moz-calc(4 / 3 * 2em)",

			"-moz-calc((2em / 4) * 3)",
			"-moz-calc((2em * 4) / 3)",
			"-moz-calc((2em * 4) * 3)",
			"-moz-calc((2em / 4) / 3)",
			"-moz-calc((4 * 2em) / 3)",
			"-moz-calc((4 / 3) * 2em)",

			"-moz-calc(2em / (4 * 3))",
			"-moz-calc(2em * (4 / 3))",
			"-moz-calc(2em * (4 * 3))",
			"-moz-calc(2em / (4 / 3))",
			"-moz-calc(4 * (2em / 3))",

			// Valid cases with unitless zero (which is never
			// a length).
			"-moz-calc(0 * 2em)",
			"-moz-calc(2em * 0)",
			"-moz-calc(3em + 0 * 2em)",
			"-moz-calc(3em + 2em * 0)",
			"-moz-calc((0 + 2) * 2em)",
			"-moz-calc((2 + 0) * 2em)",
			// And test zero lengths while we're here.
			"-moz-calc(2 * 0px)",
			"-moz-calc(0 * 0px)",
			"-moz-calc(2 * 0em)",
			"-moz-calc(0 * 0em)",
			"-moz-calc(0px * 0)",
			"-moz-calc(0px * 2)",

		],
		invalid_values: [ "20", "-1px", "red", "50%",
			/* invalid calc() values */
			"-moz-calc(2em+ 2px)",
			"-moz-calc(2em +2px)",
			"-moz-calc(2em+2px)",
			"-moz-calc(2em- 2px)",
			"-moz-calc(2em -2px)",
			"-moz-calc(2em-2px)",
			"-moz-min()",
			"-moz-calc(min())",
			"-moz-max()",
			"-moz-calc(max())",
			"-moz-min(5px)",
			"-moz-calc(min(5px))",
			"-moz-max(5px)",
			"-moz-calc(max(5px))",
			"-moz-min(5px,2em)",
			"-moz-calc(min(5px,2em))",
			"-moz-max(5px,2em)",
			"-moz-calc(max(5px,2em))",
			"-moz-calc(50px/(2 - 2))",
			"-moz-calc(5 + 5)",
			"-moz-calc(5 * 5)",
			"-moz-calc(5em * 5em)",
			"-moz-calc(5em / 5em * 5em)",

			"-moz-calc(4 * 3 / 2em)",
			"-moz-calc((4 * 3) / 2em)",
			"-moz-calc(4 * (3 / 2em))",
			"-moz-calc(4 / (3 * 2em))",

			// Tests for handling of unitless zero, which cannot
			// be a length inside calc().
			"-moz-calc(0)",
			"-moz-calc(0 + 2em)",
			"-moz-calc(2em + 0)",
			"-moz-calc(0 * 2)",
			"-moz-calc(2 * 0)",
			"-moz-calc(1 * (2em + 0))",
			"-moz-calc((2em + 0))",
			"-moz-calc((2em + 0) * 1)",
			"-moz-calc(1 * (0 + 2em))",
			"-moz-calc((0 + 2em))",
			"-moz-calc((0 + 2em) * 1)",
		]
	},
	"-moz-column-rule-style": {
		domProp: "MozColumnRuleStyle",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "solid", "hidden", "ridge", "groove", "inset", "outset", "double", "dotted", "dashed" ],
		invalid_values: [ "20", "foo" ]
	},
	"-moz-column-rule-color": {
		domProp: "MozColumnRuleColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "green" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "red", "blue", "#ffff00" ],
		invalid_values: [ ]
	},
	"-moz-column-width": {
		domProp: "MozColumnWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [
			"15px",
			"-moz-calc(15px)",
			"-moz-calc(30px - 3em)",
			"-moz-calc(-15px)",
			"0px",
			"-moz-calc(0px)"
		],
		invalid_values: [ "20", "-1px", "50%" ]
	},
	"-moz-float-edge": {
		domProp: "MozFloatEdge",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "content-box" ],
		other_values: [ "margin-box" ],
		invalid_values: [ "content", "padding", "border", "margin" ]
	},
	"-moz-force-broken-image-icon": {
		domProp: "MozForceBrokenImageIcon",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0" ],
		other_values: [ "1" ],
		invalid_values: []
	},
	"-moz-image-region": {
		domProp: "MozImageRegion",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "rect(3px 20px 15px 4px)", "rect(17px, 21px, 33px, 2px)" ],
		invalid_values: []
	},
	"-moz-margin-end": {
		domProp: "MozMarginEnd",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* no subproperties */
		/* auto may or may not be initial */
		initial_values: [ "0", "0px", "0%", "0em", "0ex", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "3em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"-moz-margin-start": {
		domProp: "MozMarginStart",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* no subproperties */
		/* auto may or may not be initial */
		initial_values: [ "0", "0px", "0%", "0em", "0ex", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "3em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"-moz-outline-radius": {
		domProp: "MozOutlineRadius",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		subproperties: [ "-moz-outline-radius-bottomleft", "-moz-outline-radius-bottomright", "-moz-outline-radius-topleft", "-moz-outline-radius-topright" ],
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px) -moz-calc(0pt) -moz-calc(0%) -moz-calc(0em)" ],
		other_values: [ "3%", "1px", "2em", "3em 2px", "2pt 3% 4em", "2px 2px 2px 2px", // circular
						"3% / 2%", "1px / 4px", "2em / 1em", "3em 2px / 2px 3em", "2pt 3% 4em / 4pt 1% 5em", "2px 2px 2px 2px / 4px 4px 4px 4px", "1pt / 2pt 3pt", "4pt 5pt / 3pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
			"2px 2px -moz-calc(2px + 1%) 2px",
			"1px 2px 2px 2px / 2px 2px -moz-calc(2px + 1%) 2px",
					  ],
		invalid_values: [ "2px -2px", "inherit 2px", "inherit / 2px", "2px inherit", "2px / inherit", "2px 2px 2px 2px 2px", "1px / 2px 2px 2px 2px 2px" ]
	},
	"-moz-outline-radius-bottomleft": {
		domProp: "MozOutlineRadiusBottomleft",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"-moz-outline-radius-bottomright": {
		domProp: "MozOutlineRadiusBottomright",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"-moz-outline-radius-topleft": {
		domProp: "MozOutlineRadiusTopleft",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"-moz-outline-radius-topright": {
		domProp: "MozOutlineRadiusTopright",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "200px", "height": "100px", "display": "inline-block"},
		initial_values: [ "0", "0px", "0%", "-moz-calc(-2px)", "-moz-calc(-1%)", "-moz-calc(0px)" ],
		other_values: [ "3%", "1px", "2em", // circular
						"3% 2%", "1px 4px", "2em 2pt", // elliptical
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3*25px) 5px",
			"5px -moz-calc(3*25px)",
			"-moz-calc(20%) -moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
					  ],
		invalid_values: [ "-1px", "4px -2px", "inherit 2px", "2px inherit" ]
	},
	"-moz-padding-end": {
		domProp: "MozPaddingEnd",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "0em", "0ex", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "3em",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"-moz-padding-start": {
		domProp: "MozPaddingStart",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		get_computed: logical_box_prop_get_computed,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "0em", "0ex", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "3em",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"resize": {
		domProp: "resize",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block", "overflow": "auto" },
		initial_values: [ "none" ],
		other_values: [ "both", "horizontal", "vertical" ],
		invalid_values: []
	},
	"-moz-stack-sizing": {
		domProp: "MozStackSizing",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "stretch-to-fit" ],
		other_values: [ "ignore" ],
		invalid_values: []
	},
	"-moz-tab-size": {
		domProp: "MozTabSize",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "8" ],
		other_values: [ "0", "3", "99", "12000" ],
		invalid_values: [ "-1", "-808", "3.0", "17.5" ]
	},
	"-moz-transform": {
		domProp: "MozTransform",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "width": "300px", "height": "50px" },
		initial_values: [ "none" ],
		other_values: [ "translatex(1px)", "translatex(4em)", "translatex(-4px)", "translatex(3px)", "translatex(0px) translatex(1px) translatex(2px) translatex(3px) translatex(4px)", "translatey(4em)", "translate(3px)", "translate(10px, -3px)", "rotate(45deg)", "rotate(45grad)", "rotate(45rad)", "rotate(0)", "scalex(10)", "scaley(10)", "scale(10)", "scale(10, 20)", "skewx(30deg)", "skewx(0)", "skewy(0)", "skewx(30grad)", "skewx(30rad)", "skewy(30deg)", "skewy(30grad)", "skewy(30rad)", "matrix(1, 2, 3, 4, 5px, 6em)", "rotate(45deg) scale(2, 1)", "skewx(45deg) skewx(-50grad)", "translate(0, 0) scale(1, 1) skewx(0) skewy(0) matrix(1, 0, 0, 1, 0, 0)", "translatex(50%)", "translatey(50%)", "translate(50%)", "translate(3%, 5px)", "translate(5px, 3%)", "matrix(1, 2, 3, 4, 5px, 6%)", "matrix(1, 2, 3, 4, 5%, 6px)", "matrix(1, 2, 3, 4, 5%, 6%)", "matrix(1, 2, 3, 4, 5, 6)",
			/* valid calc() values */
			"translatex(-moz-calc(5px + 10%))",
			"translatey(-moz-calc(0.25 * 5px + 10% / 3))",
			"translate(-moz-calc(5px - 10% * 3))",
			"translate(-moz-calc(5px - 3 * 10%), 50px)",
			"translate(-50px, -moz-calc(5px - 10% * 3))",
			"matrix(1, 0, 0, 1, -moz-calc(5px * 3), -moz-calc(10% - 3px))"
		].concat(SpecialPowers.getBoolPref("layout.3d-transforms.enabled") ? [
            "translatez(1px)", "translatez(4em)", "translatez(-4px)", "translatez(0px)", "translatez(2px) translatez(5px)", "translate3d(3px, 4px, 5px)", "translate3d(2em, 3px, 1em)", "translatex(2px) translate3d(4px, 5px, 6px) translatey(1px)", "scale3d(4, 4, 4)", "scale3d(-2, 3, -7)", "scalez(4)", "scalez(-6)", "rotate3d(2, 3, 4, 45deg)", "rotate3d(-3, 7, 0, 12rad)", "rotatex(15deg)", "rotatey(-12grad)", "rotatez(72rad)", "perspective(1000px)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13px, 14em, 15px, 16)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 20%, 10%, 15, 16)"
		] : []),
		invalid_values: ["1px", "#0000ff", "red", "auto", "translatex(1px 1px)", "translatex(translatex(1px))", "translatex(#0000ff)", "translatex(red)", "translatey()", "matrix(1px, 2px, 3px, 4px, 5px, 6px)", "scale(150%)", "skewx(red)", "matrix(1%, 0, 0, 0, 0px, 0px)", "matrix(0, 1%, 2, 3, 4px,5px)", "matrix(0, 1, 2%, 3, 4px, 5px)", "matrix(0, 1, 2, 3%, 4%, 5%)",
			/* invalid calc() values */
			"translatey(-moz-min(5px,10%))",
			"translatex(-moz-max(5px,10%))",
			"translate(10px, -moz-calc(min(5px,10%)))",
			"translate(-moz-calc(max(5px,10%)), 10%)",
			"matrix(1, 0, 0, 1, -moz-max(5px * 3), -moz-calc(10% - 3px))"
		].concat(SpecialPowers.getBoolPref("layout.3d-transforms.enabled") ? [
            "perspective(0px)", "perspective(-10px)", "matrix3d(dinosaur)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15%, 16)", "matrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16px)", "rotatey(words)", "rotatex(7)", "translate3d(3px, 4px, 1px, 7px)"
		] : [])
	},
	"-moz-transform-origin": {
		domProp: "MozTransformOrigin",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* no subproperties */
		prerequisites: { "width": "10px", "height": "10px", "display": "block"},
		initial_values: [ "50% 50%", "center", "center center" ],
		other_values: [ "25% 25%", "5px 5px", "20% 3em", "0 0", "0in 1in",
						"top", "bottom","top left", "top right",
						"top center", "center left", "center right",
						"bottom left", "bottom right", "bottom center",
						"20% center", "5px center", "13in bottom",
						"left 50px", "right 13%", "center 40px",
			"-moz-calc(20px)",
			"-moz-calc(20px) 10px",
			"10px -moz-calc(20px)",
			"-moz-calc(20px) 25%",
			"25% -moz-calc(20px)",
			"-moz-calc(20px) -moz-calc(20px)",
			"-moz-calc(20px + 1em) -moz-calc(20px / 2)",
			"-moz-calc(20px + 50%) -moz-calc(50% - 10px)",
			"-moz-calc(-20px) -moz-calc(-50%)",
			"-moz-calc(-20%) -moz-calc(-50%)"
		],
		invalid_values: ["red", "auto", "none", "0.5 0.5", "40px #0000ff",
						 "border", "center red", "right diagonal",
						 "#00ffff bottom"]
	},
	"-moz-user-focus": {
		domProp: "MozUserFocus",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "normal", "ignore", "select-all", "select-before", "select-after", "select-same", "select-menu" ],
		invalid_values: []
	},
	"-moz-user-input": {
		domProp: "MozUserInput",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "none", "enabled", "disabled" ],
		invalid_values: []
	},
	"-moz-user-modify": {
		domProp: "MozUserModify",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "read-only" ],
		other_values: [ "read-write", "write-only" ],
		invalid_values: []
	},
	"-moz-user-select": {
		domProp: "MozUserSelect",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "none", "text", "element", "elements", "all", "toggle", "tri-state", "-moz-all", "-moz-none" ],
		invalid_values: []
	},
	"-moz-window-shadow": {
		domProp: "MozWindowShadow",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "default" ],
		other_values: [ "none", "menu", "tooltip", "sheet" ],
		invalid_values: []
	},
	"background": {
		domProp: "background",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "background-attachment", "background-color", "background-image", "background-position", "background-repeat", "background-clip", "background-origin", "background-size" ],
		initial_values: [ "transparent", "none", "repeat", "scroll", "0% 0%", "top left", "left top", "transparent none", "top left none", "left top none", "none left top", "none top left", "none 0% 0%", "transparent none repeat scroll top left", "left top repeat none scroll transparent" ],
		other_values: [
				/* without multiple backgrounds */
			"green",
			"none green repeat scroll left top",
			"url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
			"repeat url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==') transparent left top scroll",
			"repeat-x",
			"repeat-y",
			"no-repeat",
			"none repeat-y transparent scroll 0% 0%",
			"fixed",
			"0% top transparent fixed repeat none",
			"top",
			"left",
			"50% 50%",
			"center",
			"bottom right scroll none transparent repeat",
			"50% transparent",
			"transparent 50%",
			"50%",
			"-moz-radial-gradient(10% bottom, #ffffff, black) scroll no-repeat",
			"-moz-linear-gradient(10px 10px -45deg, red, blue) repeat",
			"-moz-repeating-radial-gradient(10% bottom, #ffffff, black) scroll no-repeat",
			"-moz-repeating-linear-gradient(10px 10px -45deg, red, blue) repeat",
			"-moz-element(#test) lime",
				/* multiple backgrounds */
				"url(404.png), url(404.png)",
				"url(404.png), url(404.png) transparent",
				"url(404.png), url(404.png) red",
				"repeat-x, fixed, none",
				"0% top url(404.png), url(404.png) 0% top",
				"fixed repeat-y top left url(404.png), repeat-x green",
				"url(404.png), -moz-linear-gradient(20px 20px -45deg, blue, green), -moz-element(#a) black",
				/* test cases with clip+origin in the shorthand */
				"url(404.png) green padding-box",
				"url(404.png) border-box transparent",
				"content-box url(404.png) blue",
		],
		invalid_values: [
			/* mixes with keywords have to be in correct order */
			"50% left", "top 50%",
			/* bug 258080: don't accept background-position separated */
			"left url(404.png) top", "top url(404.png) left",
			/* not allowed to have color in non-bottom layer */
			"url(404.png) transparent, url(404.png)",
			"url(404.png) red, url(404.png)",
			"url(404.png) transparent, url(404.png) transparent",
			"url(404.png) transparent red, url(404.png) transparent red",
			"url(404.png) red, url(404.png) red",
			"url(404.png) rgba(0, 0, 0, 0), url(404.png)",
			"url(404.png) rgb(255, 0, 0), url(404.png)",
			"url(404.png) rgba(0, 0, 0, 0), url(404.png) rgba(0, 0, 0, 0)",
			"url(404.png) rgba(0, 0, 0, 0) rgb(255, 0, 0), url(404.png) rgba(0, 0, 0, 0) rgb(255, 0, 0)",
			"url(404.png) rgb(255, 0, 0), url(404.png) rgb(255, 0, 0)",
			/* bug 513395: old syntax for gradients */
			"-moz-radial-gradient(10% bottom, 30px, 20px 20px, 10px, from(#ffffff), to(black)) scroll no-repeat",
			"-moz-linear-gradient(10px 10px, 20px 20px, from(red), to(blue)) repeat",
		]
	},
	"background-attachment": {
		domProp: "backgroundAttachment",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "scroll" ],
		other_values: [ "fixed", "scroll,scroll", "fixed, scroll", "scroll, fixed, scroll", "fixed, fixed" ],
		invalid_values: []
	},
	"background-clip": {
		/*
		 * When we rename this to 'background-clip', we also
		 * need to rename the values to match the spec.
		 */
		domProp: "backgroundClip",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "border-box" ],
		other_values: [ "content-box", "padding-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
		invalid_values: [ "margin-box", "border-box border-box" ]
	},
	"background-color": {
		domProp: "backgroundColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "transparent", "rgba(255, 127, 15, 0)", "hsla(240, 97%, 50%, 0.0)", "rgba(0, 0, 0, 0)", "rgba(255,255,255,-3.7)" ],
		other_values: [ "green", "rgb(255, 0, 128)", "#fc2", "#96ed2a", "black", "rgba(255,255,0,3)" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000", "rgb(255.0,0.387,3489)" ]
	},
	"background-image": {
		domProp: "backgroundImage",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [
		"url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
		"none, none",
		"none, none, none, none, none",
		"url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
		"none, url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), none",
		"url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)",
		"-moz-element(#a)",
		"-moz-element(  #a  )",
		"-moz-element(#a-1)",
		"-moz-element(#a\\:1)",
		/* gradient torture test */
		"-moz-linear-gradient(red, blue)",
		"-moz-linear-gradient(red, yellow, blue)",
		"-moz-linear-gradient(red 1px, yellow 20%, blue 24em, green)",
		"-moz-linear-gradient(red, yellow, green, blue 50%)",
		"-moz-linear-gradient(red -50%, yellow -25%, green, blue)",
		"-moz-linear-gradient(red -99px, yellow, green, blue 120%)",
		"-moz-linear-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
		"-moz-linear-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

		"-moz-linear-gradient(top left, red, blue)",
		"-moz-linear-gradient(0 0, red, blue)",
		"-moz-linear-gradient(20% bottom, red, blue)",
		"-moz-linear-gradient(center 20%, red, blue)",
		"-moz-linear-gradient(left 35px, red, blue)",
		"-moz-linear-gradient(10% 10em, red, blue)",
		"-moz-linear-gradient(44px top, red, blue)",

		"-moz-linear-gradient(top left 45deg, red, blue)",
		"-moz-linear-gradient(20% bottom -300deg, red, blue)",
		"-moz-linear-gradient(center 20% 1.95929rad, red, blue)",
		"-moz-linear-gradient(left 35px 30grad, red, blue)",
		"-moz-linear-gradient(10% 10em 99999deg, red, blue)",
		"-moz-linear-gradient(44px top -33deg, red, blue)",

		"-moz-linear-gradient(-33deg, red, blue)",
		"-moz-linear-gradient(30grad left 35px, red, blue)",
		"-moz-linear-gradient(10deg 20px, red, blue)",
		"-moz-linear-gradient(.414rad bottom, red, blue)",

		"-moz-radial-gradient(red, blue)",
		"-moz-radial-gradient(red, yellow, blue)",
		"-moz-radial-gradient(red 1px, yellow 20%, blue 24em, green)",
		"-moz-radial-gradient(red, yellow, green, blue 50%)",
		"-moz-radial-gradient(red -50%, yellow -25%, green, blue)",
		"-moz-radial-gradient(red -99px, yellow, green, blue 120%)",
		"-moz-radial-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",

		"-moz-radial-gradient(top left, red, blue)",
		"-moz-radial-gradient(20% bottom, red, blue)",
		"-moz-radial-gradient(center 20%, red, blue)",
		"-moz-radial-gradient(left 35px, red, blue)",
		"-moz-radial-gradient(10% 10em, red, blue)",
		"-moz-radial-gradient(44px top, red, blue)",

		"-moz-radial-gradient(top left 45deg, red, blue)",
		"-moz-radial-gradient(0 0, red, blue)",
		"-moz-radial-gradient(20% bottom -300deg, red, blue)",
		"-moz-radial-gradient(center 20% 1.95929rad, red, blue)",
		"-moz-radial-gradient(left 35px 30grad, red, blue)",
		"-moz-radial-gradient(10% 10em 99999deg, red, blue)",
		"-moz-radial-gradient(44px top -33deg, red, blue)",
		"-moz-radial-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

		"-moz-radial-gradient(-33deg, red, blue)",
		"-moz-radial-gradient(30grad left 35px, red, blue)",
		"-moz-radial-gradient(10deg 20px, red, blue)",
		"-moz-radial-gradient(.414rad bottom, red, blue)",

		"-moz-radial-gradient(cover, red, blue)",
		"-moz-radial-gradient(circle, red, blue)",
		"-moz-radial-gradient(ellipse closest-corner, red, blue)",
		"-moz-radial-gradient(farthest-side circle, red, blue)",

		"-moz-radial-gradient(top left, cover, red, blue)",
		"-moz-radial-gradient(15% 20%, circle, red, blue)",
		"-moz-radial-gradient(45px, ellipse closest-corner, red, blue)",
		"-moz-radial-gradient(45px, farthest-side circle, red, blue)",

		"-moz-radial-gradient(99deg, cover, red, blue)",
		"-moz-radial-gradient(-1.2345rad, circle, red, blue)",
		"-moz-radial-gradient(399grad, ellipse closest-corner, red, blue)",
		"-moz-radial-gradient(399grad, farthest-side circle, red, blue)",

		"-moz-radial-gradient(top left 99deg, cover, red, blue)",
		"-moz-radial-gradient(15% 20% -1.2345rad, circle, red, blue)",
		"-moz-radial-gradient(45px 399grad, ellipse closest-corner, red, blue)",
		"-moz-radial-gradient(45px 399grad, farthest-side circle, red, blue)",

		"-moz-repeating-linear-gradient(red, blue)",
		"-moz-repeating-linear-gradient(red, yellow, blue)",
		"-moz-repeating-linear-gradient(red 1px, yellow 20%, blue 24em, green)",
		"-moz-repeating-linear-gradient(red, yellow, green, blue 50%)",
		"-moz-repeating-linear-gradient(red -50%, yellow -25%, green, blue)",
		"-moz-repeating-linear-gradient(red -99px, yellow, green, blue 120%)",
		"-moz-repeating-linear-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
		"-moz-repeating-linear-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

		"-moz-repeating-linear-gradient(top left, red, blue)",
		"-moz-repeating-linear-gradient(0 0, red, blue)",
		"-moz-repeating-linear-gradient(20% bottom, red, blue)",
		"-moz-repeating-linear-gradient(center 20%, red, blue)",
		"-moz-repeating-linear-gradient(left 35px, red, blue)",
		"-moz-repeating-linear-gradient(10% 10em, red, blue)",
		"-moz-repeating-linear-gradient(44px top, red, blue)",

		"-moz-repeating-linear-gradient(top left 45deg, red, blue)",
		"-moz-repeating-linear-gradient(20% bottom -300deg, red, blue)",
		"-moz-repeating-linear-gradient(center 20% 1.95929rad, red, blue)",
		"-moz-repeating-linear-gradient(left 35px 30grad, red, blue)",
		"-moz-repeating-linear-gradient(10% 10em 99999deg, red, blue)",
		"-moz-repeating-linear-gradient(44px top -33deg, red, blue)",

		"-moz-repeating-linear-gradient(-33deg, red, blue)",
		"-moz-repeating-linear-gradient(30grad left 35px, red, blue)",
		"-moz-repeating-linear-gradient(10deg 20px, red, blue)",
		"-moz-repeating-linear-gradient(.414rad bottom, red, blue)",

		"-moz-repeating-radial-gradient(red, blue)",
		"-moz-repeating-radial-gradient(red, yellow, blue)",
		"-moz-repeating-radial-gradient(red 1px, yellow 20%, blue 24em, green)",
		"-moz-repeating-radial-gradient(red, yellow, green, blue 50%)",
		"-moz-repeating-radial-gradient(red -50%, yellow -25%, green, blue)",
		"-moz-repeating-radial-gradient(red -99px, yellow, green, blue 120%)",
		"-moz-repeating-radial-gradient(#ffff00, #ef3, rgba(10, 20, 30, 0.4))",
		"-moz-repeating-radial-gradient(rgba(10, 20, 30, 0.4), #ffff00, #ef3)",

		"-moz-repeating-radial-gradient(top left, red, blue)",
		"-moz-repeating-radial-gradient(0 0, red, blue)",
		"-moz-repeating-radial-gradient(20% bottom, red, blue)",
		"-moz-repeating-radial-gradient(center 20%, red, blue)",
		"-moz-repeating-radial-gradient(left 35px, red, blue)",
		"-moz-repeating-radial-gradient(10% 10em, red, blue)",
		"-moz-repeating-radial-gradient(44px top, red, blue)",

		"-moz-repeating-radial-gradient(top left 45deg, red, blue)",
		"-moz-repeating-radial-gradient(20% bottom -300deg, red, blue)",
		"-moz-repeating-radial-gradient(center 20% 1.95929rad, red, blue)",
		"-moz-repeating-radial-gradient(left 35px 30grad, red, blue)",
		"-moz-repeating-radial-gradient(10% 10em 99999deg, red, blue)",
		"-moz-repeating-radial-gradient(44px top -33deg, red, blue)",

		"-moz-repeating-radial-gradient(-33deg, red, blue)",
		"-moz-repeating-radial-gradient(30grad left 35px, red, blue)",
		"-moz-repeating-radial-gradient(10deg 20px, red, blue)",
		"-moz-repeating-radial-gradient(.414rad bottom, red, blue)",

		"-moz-repeating-radial-gradient(cover, red, blue)",
		"-moz-repeating-radial-gradient(circle, red, blue)",
		"-moz-repeating-radial-gradient(ellipse closest-corner, red, blue)",
		"-moz-repeating-radial-gradient(farthest-side circle, red, blue)",

		"-moz-repeating-radial-gradient(top left, cover, red, blue)",
		"-moz-repeating-radial-gradient(15% 20%, circle, red, blue)",
		"-moz-repeating-radial-gradient(45px, ellipse closest-corner, red, blue)",
		"-moz-repeating-radial-gradient(45px, farthest-side circle, red, blue)",

		"-moz-repeating-radial-gradient(99deg, cover, red, blue)",
		"-moz-repeating-radial-gradient(-1.2345rad, circle, red, blue)",
		"-moz-repeating-radial-gradient(399grad, ellipse closest-corner, red, blue)",
		"-moz-repeating-radial-gradient(399grad, farthest-side circle, red, blue)",

		"-moz-repeating-radial-gradient(top left 99deg, cover, red, blue)",
		"-moz-repeating-radial-gradient(15% 20% -1.2345rad, circle, red, blue)",
		"-moz-repeating-radial-gradient(45px 399grad, ellipse closest-corner, red, blue)",
		"-moz-repeating-radial-gradient(45px 399grad, farthest-side circle, red, blue)",
		"-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 2, 10, 10, 2)",
		"-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 10%, 50%, 30%, 0%)",
		"-moz-image-rect(url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==), 10, 50%, 30%, 0)",

		"-moz-radial-gradient(-moz-calc(25%) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(25%), red, blue)",
		"-moz-radial-gradient(-moz-calc(25px) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(25px), red, blue)",
		"-moz-radial-gradient(-moz-calc(-25%) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(-25%), red, blue)",
		"-moz-radial-gradient(-moz-calc(-25px) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(-25px), red, blue)",
		"-moz-radial-gradient(-moz-calc(100px + -25%) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(100px + -25%), red, blue)",
		"-moz-radial-gradient(-moz-calc(100px + -25px) top, red, blue)",
		"-moz-radial-gradient(left -moz-calc(100px + -25px), red, blue)",
		],
		invalid_values: [
			"-moz-element(#a:1)",
			"-moz-element(a#a)",
			"-moz-element(#a a)",
			"-moz-element(#a+a)",
			"-moz-element(#a()",
			/* Old syntax */
			"-moz-linear-gradient(10px 10px, 20px, 30px 30px, 40px, from(blue), to(red))",
			"-moz-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
			"-moz-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
			"-moz-linear-gradient(10px, 20px, 30px, 40px, color-stop(0.5, #00ccff))",
			"-moz-linear-gradient(20px 20px, from(blue), to(red))",
			"-moz-linear-gradient(40px 40px, 10px 10px, from(blue) to(red) color-stop(10%, fuchsia))",
			"-moz-linear-gradient(20px 20px 30px, 10px 10px, from(red), to(#ff0000))",
			"-moz-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
			"-moz-linear-gradient(left left, top top, from(blue))",
			"-moz-linear-gradient(inherit, 10px 10px, from(blue))",
			/* New syntax */
			"-moz-linear-gradient(10px 10px, 20px, 30px 30px, 40px, blue 0, red 100%)",
			"-moz-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
			"-moz-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
			"-moz-linear-gradient(10px, 20px, 30px, 40px, #00ccff 50%)",
			"-moz-linear-gradient(40px 40px, 10px 10px, blue 0 fuchsia 10% red 100%)",
			"-moz-linear-gradient(20px 20px 30px, 10px 10px, red 0, #ff0000 100%)",
			"-moz-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
			"-moz-linear-gradient(left left, top top, blue 0)",
			"-moz-linear-gradient(inherit, 10px 10px, blue 0)",
			"-moz-linear-gradient(left left blue red)",
			"-moz-linear-gradient(left left blue, red)",
			"-moz-linear-gradient()",
			"-moz-linear-gradient(cover, red, blue)",
			"-moz-linear-gradient(auto, red, blue)",
			"-moz-linear-gradient(22 top, red, blue)",
			"-moz-linear-gradient(10% red blue)",
			"-moz-linear-gradient(10%, red blue)",
			"-moz-linear-gradient(10%,, red, blue)",
			"-moz-linear-gradient(45px, center, red, blue)",
			"-moz-linear-gradient(45px, center red, blue)",
			"-moz-radial-gradient(contain, ellipse, red, blue)",
			"-moz-radial-gradient(10deg contain, red, blue)",
			"-moz-radial-gradient(10deg, contain,, red, blue)",
			"-moz-radial-gradient(contain contain, red, blue)",
			"-moz-radial-gradient(ellipse circle, red, blue)",

			"-moz-repeating-linear-gradient(10px 10px, 20px, 30px 30px, 40px, blue 0, red 100%)",
			"-moz-repeating-radial-gradient(20px 20px, 10px 10px, from(green), to(#ff00ff))",
			"-moz-repeating-radial-gradient(10px 10px, 20%, 40px 40px, 10px, from(green), to(#ff00ff))",
			"-moz-repeating-linear-gradient(10px, 20px, 30px, 40px, #00ccff 50%)",
			"-moz-repeating-linear-gradient(40px 40px, 10px 10px, blue 0 fuchsia 10% red 100%)",
			"-moz-repeating-linear-gradient(20px 20px 30px, 10px 10px, red 0, #ff0000 100%)",
			"-moz-repeating-radial-gradient(left top, center, 20px 20px, 10px, from(blue), to(red))",
			"-moz-repeating-linear-gradient(left left, top top, blue 0)",
			"-moz-repeating-linear-gradient(inherit, 10px 10px, blue 0)",
			"-moz-repeating-linear-gradient(left left blue red)",
			"-moz-repeating-linear-gradient()" ]
	},
	"background-origin": {
		domProp: "backgroundOrigin",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "padding-box" ],
		other_values: [ "border-box", "content-box", "border-box, padding-box", "padding-box, padding-box, padding-box", "border-box, border-box" ],
		invalid_values: [ "margin-box", "padding-box padding-box" ]
	},
	"background-position": {
		domProp: "backgroundPosition",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "top left", "left top", "0% 0%", "0% top", "left 0%" ],
		other_values: [ "top", "left", "right", "bottom", "center", "center bottom", "bottom center", "center right", "right center", "center top", "top center", "center left", "left center", "right bottom", "bottom right", "50%", "top left, top left", "top left, top right", "top right, top left", "left top, 0% 0%", "10% 20%, 30%, 40%", "top left, bottom right", "right bottom, left top", "0%", "0px", "30px", "0%, 10%, 20%, 30%", "top, top, top, top, top",
			"-moz-calc(20px)",
			"-moz-calc(20px) 10px",
			"10px -moz-calc(20px)",
			"-moz-calc(20px) 25%",
			"25% -moz-calc(20px)",
			"-moz-calc(20px) -moz-calc(20px)",
			"-moz-calc(20px + 1em) -moz-calc(20px / 2)",
			"-moz-calc(20px + 50%) -moz-calc(50% - 10px)",
			"-moz-calc(-20px) -moz-calc(-50%)",
			"-moz-calc(-20%) -moz-calc(-50%)",
			"0px 0px"
		],
		invalid_values: [ "50% left", "top 50%" ]
	},
	"background-repeat": {
		domProp: "backgroundRepeat",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "repeat" ],
		other_values: [ "repeat-x", "repeat-y", "no-repeat",
			"repeat-x, repeat-x",
			"repeat, no-repeat",
			"repeat-y, no-repeat, repeat-y",
			"repeat, repeat, repeat"
		],
		invalid_values: [ "repeat repeat" ]
	},
	"background-size": {
		domProp: "backgroundSize",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto", "auto auto" ],
		other_values: [ "contain", "cover", "100px auto", "auto 100px", "100% auto", "auto 100%", "25% 50px", "3em 40%",
			"-moz-calc(20px)",
			"-moz-calc(20px) 10px",
			"10px -moz-calc(20px)",
			"-moz-calc(20px) 25%",
			"25% -moz-calc(20px)",
			"-moz-calc(20px) -moz-calc(20px)",
			"-moz-calc(20px + 1em) -moz-calc(20px / 2)",
			"-moz-calc(20px + 50%) -moz-calc(50% - 10px)",
			"-moz-calc(-20px) -moz-calc(-50%)",
			"-moz-calc(-20%) -moz-calc(-50%)"
		],
		invalid_values: [ "contain contain", "cover cover", "cover auto", "auto cover", "contain cover", "cover contain", "-5px 3px", "3px -5px", "auto -5px", "-5px auto" ]
	},
	"border": {
		domProp: "border",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width", "border-left-color", "border-left-style", "border-left-width", "border-right-color", "border-right-style", "border-right-width", "border-top-color", "border-top-style", "border-top-width", "-moz-border-top-colors", "-moz-border-right-colors", "-moz-border-bottom-colors", "-moz-border-left-colors", "-moz-border-image" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor", "-moz-calc(4px - 1px) none" ],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid", "-moz-calc(2px) solid blue" ],
		invalid_values: [ "5%" ]
	},
	"border-bottom": {
		domProp: "borderBottom",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"border-bottom-color": {
		domProp: "borderBottomColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-bottom-style": {
		domProp: "borderBottomStyle",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-bottom-width": {
		domProp: "borderBottomWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "border-bottom-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"border-collapse": {
		domProp: "borderCollapse",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "separate" ],
		other_values: [ "collapse" ],
		invalid_values: []
	},
	"border-color": {
		domProp: "borderColor",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-color", "border-right-color", "border-bottom-color", "border-left-color" ],
		initial_values: [ "currentColor", "currentColor currentColor", "currentColor currentColor currentColor", "currentColor currentColor currentcolor CURRENTcolor" ],
		other_values: [ "green", "currentColor green", "currentColor currentColor green", "currentColor currentColor currentColor green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-left": {
		domProp: "borderLeft",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-left-color", "border-left-style", "border-left-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"border-left-color": {
		domProp: "borderLeftColor",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-left-style": {
		domProp: "borderLeftStyle",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-left-width": {
		domProp: "borderLeftWidth",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		prerequisites: { "border-left-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"border-right": {
		domProp: "borderRight",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-right-color", "border-right-style", "border-right-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"border-right-color": {
		domProp: "borderRightColor",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-right-style": {
		domProp: "borderRightStyle",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-right-width": {
		domProp: "borderRightWidth",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		prerequisites: { "border-right-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"border-spacing": {
		domProp: "borderSpacing",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0 0", "0px", "0 0px", "-moz-calc(0px)", "-moz-calc(0px) -moz-calc(0em)", "-moz-calc(2em - 2em) -moz-calc(3px + 7px - 10px)", "-moz-calc(-5px)", "-moz-calc(-5px) -moz-calc(-5px)" ],
		other_values: [ "3px", "4em 2px", "4em 0", "0px 2px", "-moz-calc(7px)", "0 -moz-calc(7px)", "-moz-calc(7px) 0", "-moz-calc(0px) -moz-calc(7px)", "-moz-calc(7px) -moz-calc(0px)", "7px -moz-calc(0px)", "-moz-calc(0px) 7px", "7px -moz-calc(0px)", "3px -moz-calc(2em)" ],
		invalid_values: [ "0%", "0 0%", "-5px", "-5px -5px", "0 -5px", "-5px 0" ]
	},
	"border-style": {
		domProp: "borderStyle",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-style", "border-right-style", "border-bottom-style", "border-left-style" ],
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none", "none none", "none none none", "none none none none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge", "none solid", "none none solid", "none none none solid", "groove none none none", "none ridge none none", "none none double none", "none none none dotted" ],
		invalid_values: []
	},
	"border-top": {
		domProp: "borderTop",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-color", "border-top-style", "border-top-width" ],
		initial_values: [ "none", "medium", "currentColor", "thin", "none medium currentcolor" ],
		other_values: [ "solid", "green", "medium solid", "green solid", "10px solid", "thick solid", "5px green none" ],
		invalid_values: [ "5%" ]
	},
	"border-top-color": {
		domProp: "borderTopColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-top-style": {
		domProp: "borderTopStyle",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-top-width": {
		domProp: "borderTopWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "border-top-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0em)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"border-width": {
		domProp: "borderWidth",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-width", "border-right-width", "border-bottom-width", "border-left-width" ],
		prerequisites: { "border-style": "solid" },
		initial_values: [ "medium", "3px", "medium medium", "3px medium medium", "medium 3px medium medium", "-moz-calc(3px) 3px -moz-calc(5px - 2px) -moz-calc(2px - -1px)" ],
		other_values: [ "thin", "thick", "1px", "2em", "2px 0 0px 1em", "-moz-calc(2em)" ],
		invalid_values: [ "5%" ]
	},
	"bottom": {
		domProp: "bottom",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* FIXME: run tests with multiple prerequisites */
		prerequisites: { "position": "relative" },
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"box-shadow": {
		domProp: "boxShadow",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		prerequisites: { "color": "blue" },
		other_values: [ "2px 2px", "2px 2px 1px", "2px 2px 2px 2px", "blue 3px 2px", "2px 2px 1px 5px green", "2px 2px red", "green 2px 2px 1px", "green 2px 2px, blue 1px 3px 4px", "currentColor 3px 3px", "blue 2px 2px, currentColor 1px 2px, 1px 2px 3px 2px orange", "3px 0 0 0", "inset 2px 2px 3px 4px black", "2px -2px green inset, 4px 4px 3px blue, inset 2px 2px",
			/* calc() values */
			"2px 2px -moz-calc(-5px)", /* clamped */
			"-moz-calc(3em - 2px) 2px green",
			"green -moz-calc(3em - 2px) 2px",
			"2px -moz-calc(2px + 0.2em)",
			"blue 2px -moz-calc(2px + 0.2em)",
			"2px -moz-calc(2px + 0.2em) blue",
			"-moz-calc(-2px) -moz-calc(-2px)",
			"-2px -2px",
			"-moz-calc(2px) -moz-calc(2px)",
			"-moz-calc(2px) -moz-calc(2px) -moz-calc(2px)",
			"-moz-calc(2px) -moz-calc(2px) -moz-calc(2px) -moz-calc(2px)"
		],
		invalid_values: [ "3% 3%", "1px 1px 1px 1px 1px", "2px 2px, none", "red 2px 2px blue", "inherit, 2px 2px", "2px 2px, inherit", "2px 2px -5px", "inset 4px 4px black inset", "inset inherit", "inset none" ]
	},
	"caption-side": {
		domProp: "captionSide",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "top" ],
		other_values: [ "right", "left", "bottom", "top-outside", "bottom-outside" ],
		invalid_values: []
	},
	"clear": {
		domProp: "clear",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "left", "right", "both" ],
		invalid_values: []
	},
	"clip": {
		domProp: "clip",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "rect(0 0 0 0)", "rect(auto,auto,auto,auto)", "rect(3px, 4px, 4em, 0)", "rect(auto, 3em, 4pt, 2px)", "rect(2px 3px 4px 5px)" ],
		invalid_values: [ "rect(auto, 3em, 2%, 5px)" ]
	},
	"color": {
		domProp: "color",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		/* XXX should test currentColor, but may or may not be initial */
		initial_values: [ "black", "#000" ],
		other_values: [ "green", "#f3c", "#fed292", "rgba(45,300,12,2)", "transparent", "-moz-nativehyperlinktext", "rgba(255,128,0,0.5)" ],
		invalid_values: [ "fff", "ffffff", "#f", "#ff", "#ffff", "#fffff", "#fffffff", "#ffffffff", "#fffffffff" ]
	},
	"content": {
		domProp: "content",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX needs to be on pseudo-elements */
		initial_values: [ "normal", "none" ],
		other_values: [ '""', "''", '"hello"', "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")', 'counter(foo)', 'counter(bar, upper-roman)', 'counters(foo, ".")', "counters(bar, '-', lower-greek)", "'-' counter(foo) '.'", "attr(title)", "open-quote", "close-quote", "no-open-quote", "no-close-quote", "close-quote attr(title) counters(foo, '.', upper-alpha)", "counter(foo, none)", "counters(bar, '.', none)", "attr(\\32)", "attr(\\2)", "attr(-\\2)", "attr(-\\32)", "counter(\\2)", "counters(\\32, '.')", "counter(-\\32, upper-roman)", "counters(-\\2, '-', lower-greek)", "counter(\\()", "counters(a\\+b, '.')", "counter(\\}, upper-alpha)", "-moz-alt-content" ],
		invalid_values: [ 'counters(foo)', 'counter(foo, ".")', 'attr("title")', "attr('title')", "attr(2)", "attr(-2)", "counter(2)", "counters(-2, '.')", "-moz-alt-content 'foo'", "'foo' -moz-alt-content" ]
	},
	"counter-increment": {
		domProp: "counterIncrement",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "foo 1", "bar", "foo 3 bar baz 2", "\\32  1", "-\\32  1", "-c 1", "\\32 1", "-\\32 1", "\\2  1", "-\\2  1", "-c 1", "\\2 1", "-\\2 1" ],
		invalid_values: []
	},
	"counter-reset": {
		domProp: "counterReset",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "foo 1", "bar", "foo 3 bar baz 2", "\\32  1", "-\\32  1", "-c 1", "\\32 1", "-\\32 1", "\\2  1", "-\\2  1", "-c 1", "\\2 1", "-\\2 1" ],
		invalid_values: []
	},
	"cursor": {
		domProp: "cursor",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "crosshair", "default", "pointer", "move", "e-resize", "ne-resize", "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize", "w-resize", "text", "wait", "help", "progress", "copy", "alias", "context-menu", "cell", "not-allowed", "col-resize", "row-resize", "no-drop", "vertical-text", "all-scroll", "nesw-resize", "nwse-resize", "ns-resize", "ew-resize", "none", "-moz-grab", "-moz-grabbing", "-moz-zoom-in", "-moz-zoom-out" ],
		invalid_values: []
	},
	"direction": {
		domProp: "direction",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "ltr" ],
		other_values: [ "rtl" ],
		invalid_values: []
	},
	"display": {
		domProp: "display",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "inline" ],
		/* XXX none will really mess with other properties */
		prerequisites: { "float": "none", "position": "static" },
		other_values: [ "block", "list-item", "inline-block", "table", "inline-table", "table-row-group", "table-header-group", "table-footer-group", "table-row", "table-column-group", "table-column", "table-cell", "table-caption", "none" ],
		invalid_values: []
	},
	"empty-cells": {
		domProp: "emptyCells",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "show" ],
		other_values: [ "hide" ],
		invalid_values: []
	},
	"float": {
		domProp: "cssFloat",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "left", "right" ],
		invalid_values: []
	},
	"font": {
		domProp: "font",
		inherited: true,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "font-style", "font-variant", "font-weight", "font-size", "line-height", "font-family", "font-stretch", "font-size-adjust", "-moz-font-feature-settings", "-moz-font-language-override" ],
		initial_values: [ (gInitialFontFamilyIsSansSerif ? "medium sans-serif" : "medium serif") ],
		other_values: [ "large serif", "9px fantasy", "bold italic small-caps 24px/1.4 Times New Roman, serif", "caption", "icon", "menu", "message-box", "small-caption", "status-bar" ],
		invalid_values: []
	},
	"font-family": {
		domProp: "fontFamily",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ (gInitialFontFamilyIsSansSerif ? "sans-serif" : "serif") ],
		other_values: [ (gInitialFontFamilyIsSansSerif ? "serif" : "sans-serif"), "Times New Roman, serif", "'Times New Roman', serif", "cursive", "fantasy", "\\\"Times New Roman", "\"Times New Roman\"", "Times, \\\"Times New Roman", "Times, \"Times New Roman\"" ],
		invalid_values: [ "\"Times New\" Roman", "\"Times New Roman\n", "Times, \"Times New Roman\n" ]
	},
	"-moz-font-feature-settings": {
		domProp: "MozFontFeatureSettings",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "'liga=1'", "\"liga=1\"", "'foo,bar=\"hello\"'" ],
		invalid_values: [ "liga=1", "foo,bar=\"hello\"" ]
	},
	"-moz-font-language-override": {
		domProp: "MozFontLanguageOverride",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "'TRK'", "\"TRK\"", "'N\\'Ko'" ],
		invalid_values: [ "TRK" ]
	},
	"font-size": {
		domProp: "fontSize",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "medium",
			"1rem",
			"-moz-calc(1rem)",
			"-moz-calc(0.75rem + 200% - 125% + 0.25rem - 75%)"
		],
		other_values: [ "large", "2em", "50%", "xx-small", "36pt", "8px",
			"0px",
			"0%",
			"-moz-calc(2em)",
			"-moz-calc(36pt + 75% + (30% + 2em + 2px))",
			"-moz-calc(-2em)",
			"-moz-calc(-50%)",
			"-moz-calc(-1px)"
		],
		invalid_values: [ "-2em", "-50%", "-1px" ]
	},
	"font-size-adjust": {
		domProp: "fontSizeAdjust",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "0.3", "0.5", "0.7" ],
		invalid_values: []
	},
	"font-stretch": {
		domProp: "fontStretch",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded" ],
		invalid_values: []
	},
	"font-style": {
		domProp: "fontStyle",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "italic", "oblique" ],
		invalid_values: []
	},
	"font-variant": {
		domProp: "fontVariant",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "small-caps" ],
		invalid_values: []
	},
	"font-weight": {
		domProp: "fontWeight",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal", "400" ],
		other_values: [ "bold", "100", "200", "300", "500", "600", "700", "800", "900", "bolder", "lighter" ],
		invalid_values: [ "107", "399", "401", "699", "710" ]
	},
	"height": {
		domProp: "height",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* FIXME: test zero, and test calc clamping */
		initial_values: [ " auto" ],
		/* computed value tests for height test more with display:block */
		prerequisites: { "display": "block" },
		other_values: [ "15px", "3em", "15%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "none", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ]
	},
	"ime-mode": {
		domProp: "imeMode",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "normal", "disabled", "active", "inactive" ],
		invalid_values: [ "none", "enabled", "1px" ]
	},
	"left": {
		domProp: "left",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* FIXME: run tests with multiple prerequisites */
		prerequisites: { "position": "relative" },
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"letter-spacing": {
		domProp: "letterSpacing",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "0", "0px", "1em", "2px", "-3px",
			"-moz-calc(0px)", "-moz-calc(1em)", "-moz-calc(1em + 3px)",
			"-moz-calc(15px / 2)", "-moz-calc(15px/2)", "-moz-calc(-3px)"
		],
		invalid_values: []
	},
	"line-height": {
		domProp: "lineHeight",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		/*
		 * Inheritance tests require consistent font size, since
		 * getComputedStyle (which uses the CSS2 computed value, or
		 * CSS2.1 used value) doesn't match what the CSS2.1 computed
		 * value is.  And they even require consistent font metrics for
		 * computation of 'normal'.	 -moz-block-height requires height
		 * on a block.
		 */
		prerequisites: { "font-size": "19px", "font-size-adjust": "none", "font-family": "serif", "font-weight": "normal", "font-style": "normal", "height": "18px", "display": "block"},
		initial_values: [ "normal" ],
		other_values: [ "1.0", "1", "1em", "47px", "-moz-block-height" ],
		invalid_values: []
	},
	"list-style": {
		domProp: "listStyle",
		inherited: true,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "list-style-type", "list-style-position", "list-style-image" ],
		initial_values: [ "outside", "disc", "disc outside", "outside disc", "disc none", "none disc", "none disc outside", "none outside disc", "disc none outside", "disc outside none", "outside none disc", "outside disc none" ],
		other_values: [ "inside none", "none inside", "none none inside", "square", "none", "none none", "outside none none", "none outside none", "none none outside", "none outside", "outside none",
			'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			'none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none',
			'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside',
			'outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			'outside none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			'outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none',
			'none url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside',
			'none outside url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") outside none',
			'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==") none outside'
		],
		invalid_values: [ "outside outside", "disc disc", "unknown value", "none none none", "none disc url(404.png)", "none url(404.png) disc", "disc none url(404.png)", "disc url(404.png) none", "url(404.png) none disc", "url(404.png) disc none", "none disc outside url(404.png)" ]
	},
	"list-style-image": {
		domProp: "listStyleImage",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")',
			// Add some tests for interesting url() values here to test serialization, etc.
			"url(\'data:text/plain,\"\')",
			"url(\"data:text/plain,\'\")",
			"url(\'data:text/plain,\\\'\')",
			"url(\"data:text/plain,\\\"\")",
			"url(\'data:text/plain,\\\"\')",
			"url(\"data:text/plain,\\\'\")",
			"url(data:text/plain,\\\\)",
		],
		invalid_values: []
	},
	"list-style-position": {
		domProp: "listStylePosition",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "outside" ],
		other_values: [ "inside" ],
		invalid_values: []
	},
	"list-style-type": {
		domProp: "listStyleType",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "disc" ],
		other_values: [ "circle", "decimal-leading-zero", "upper-alpha" ],
		invalid_values: []
	},
	"margin": {
		domProp: "margin",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "margin-top", "margin-right", "margin-bottom", "margin-left" ],
		initial_values: [ "0", "0px 0 0em", "0% 0px 0em 0pt" ],
		other_values: [ "3px 0", "2em 4px 2pt", "1em 2em 3px 4px" ],
		invalid_values: []
	},
	"margin-bottom": {
		domProp: "marginBottom",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"margin-left": {
		domProp: "marginLeft",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "2em", "5%", ".5px", "+32px", "+.789px", "-.328px", "+0.56px", "-0.974px", "237px", "-289px", "-056px", "1987.45px", "-84.32px",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "..25px", ".+5px", ".px", "-.px", "++5px", "-+4px", "+-3px", "--7px", "+-.6px", "-+.5px", "++.7px", "--.4px" ]
	},
	"margin-right": {
		domProp: "marginRight",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"margin-top": {
		domProp: "marginTop",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"marker-offset": {
		domProp: "markerOffset",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "6em", "-1px", "-moz-calc(0px)", "-moz-calc(3em + 2px - 4px)", "-moz-calc(-2em)" ],
		invalid_values: []
	},
	"marks": {
		/* XXX not a real property; applies only to page context */
		domProp: "marks",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "crop", "cross", "crop cross", "cross crop" ],
		invalid_values: [ "none none", "crop none", "none crop", "cross none", "none cross" ]
	},
	"max-height": {
		domProp: "maxHeight",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block" },
		initial_values: [ "none" ],
		other_values: [ "30px", "50%", "0",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "auto", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ]
	},
	"max-width": {
		domProp: "maxWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block" },
		initial_values: [ "none" ],
		other_values: [ "30px", "50%", "0", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "auto" ]
	},
	"min-height": {
		domProp: "minHeight",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block" },
		initial_values: [ "0", "-moz-calc(0em)", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "30px", "50%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "auto", "none", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available" ]
	},
	"min-width": {
		domProp: "minWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block" },
		initial_values: [ "0", "-moz-calc(0em)", "-moz-calc(-2px)", "-moz-calc(-1%)" ],
		other_values: [ "30px", "50%", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ "auto", "none" ]
	},
	"opacity": {
		domProp: "opacity",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1", "17", "397.376" ],
		other_values: [ "0", "0.4", "0.0000", "-3" ],
		invalid_values: [ "0px", "1px" ]
	},
	"-moz-orient": {
		domProp: "MozOrient",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "horizontal" ],
		other_values: [ "vertical" ],
		invalid_values: [ "auto", "none" ]
	},
	"orphans": {
		domProp: "orphans",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		// XXX requires display:block
		initial_values: [ "2" ],
		other_values: [ "1", "7" ],
		invalid_values: [ "0", "-1", "0px", "3px" ]
	},
	"outline": {
		domProp: "outline",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "outline-color", "outline-style", "outline-width" ],
		initial_values: [
			"none", "medium", "thin",
			// XXX Should be invert, but currently currentcolor.
			//"invert", "none medium invert"
			"currentColor", "none medium currentcolor"
		],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"outline-color": {
		domProp: "outlineColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ], // XXX should be invert
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"outline-offset": {
		domProp: "outlineOffset",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "-0", "-moz-calc(0px)", "-moz-calc(3em + 2px - 2px - 3em)", "-moz-calc(-0em)" ],
		other_values: [ "-3px", "1em", "-moz-calc(3em)", "-moz-calc(7pt + 3 * 2em)", "-moz-calc(-3px)" ],
		invalid_values: [ "5%" ]
	},
	"outline-style": {
		domProp: "outlineStyle",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		// XXX Should 'hidden' be the same as initial?
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"outline-width": {
		domProp: "outlineWidth",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "outline-style": "solid" },
		initial_values: [ "medium", "3px", "-moz-calc(4px - 1px)" ],
		other_values: [ "thin", "thick", "1px", "2em",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(0px)",
			"-moz-calc(0px)",
			"-moz-calc(5em)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 5em)",
		],
		invalid_values: [ "5%" ]
	},
	"overflow": {
		domProp: "overflow",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		prerequisites: { "display": "block" },
		subproperties: [ "overflow-x", "overflow-y" ],
		initial_values: [ "visible" ],
		other_values: [ "auto", "scroll", "hidden" ],
		invalid_values: []
	},
	"overflow-x": {
		domProp: "overflowX",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block", "overflow-y": "visible" },
		initial_values: [ "visible" ],
		other_values: [ "auto", "scroll", "hidden" ],
		invalid_values: []
	},
	"overflow-y": {
		domProp: "overflowY",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "display": "block", "overflow-x": "visible" },
		initial_values: [ "visible" ],
		other_values: [ "auto", "scroll", "hidden" ],
		invalid_values: []
	},
	"padding": {
		domProp: "padding",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "padding-top", "padding-right", "padding-bottom", "padding-left" ],
		initial_values: [ "0", "0px 0 0em", "0% 0px 0em 0pt", "-moz-calc(0px) -moz-calc(0em) -moz-calc(-2px) -moz-calc(-1%)" ],
		other_values: [ "3px 0", "2em 4px 2pt", "1em 2em 3px 4px" ],
		invalid_values: []
	},
	"padding-bottom": {
		domProp: "paddingBottom",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"padding-left": {
		domProp: "paddingLeft",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"padding-right": {
		domProp: "paddingRight",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"padding-top": {
		domProp: "paddingTop",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%", "-moz-calc(0pt)", "-moz-calc(0% + 0px)", "-moz-calc(-3px)", "-moz-calc(-1%)" ],
		other_values: [ "1px", "2em", "5%",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: [ ]
	},
	"page": {
		domProp: "page",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "foo", "bar" ],
		invalid_values: [ "3px" ]
	},
	"page-break-after": {
		domProp: "pageBreakAfter",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "always", "avoid", "left", "right" ],
		invalid_values: []
	},
	"page-break-before": {
		domProp: "pageBreakBefore",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "always", "avoid", "left", "right" ],
		invalid_values: []
	},
	"page-break-inside": {
		domProp: "pageBreakInside",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "avoid" ],
		invalid_values: []
	},
	"pointer-events": {
		domProp: "pointerEvents",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "visiblePainted", "visibleFill", "visibleStroke", "visible",
						"painted", "fill", "stroke", "all", "none" ],
		invalid_values: []
	},
	"position": {
		domProp: "position",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "static" ],
		other_values: [ "relative", "absolute", "fixed" ],
		invalid_values: []
	},
	"quotes": {
		domProp: "quotes",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ '"\u201C" "\u201D" "\u2018" "\u2019"',
						  '"\\201C" "\\201D" "\\2018" "\\2019"' ],
		other_values: [ "none", "'\"' '\"'" ],
		invalid_values: []
	},
	"right": {
		domProp: "right",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* FIXME: run tests with multiple prerequisites */
		prerequisites: { "position": "relative" },
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"size": {
		/* XXX not a real property; applies only to page context */
		domProp: "size",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "landscape", "portrait", "8.5in 11in", "14in 11in", "297mm 210mm", "21cm 29.7cm", "100mm" ],
		invalid_values: [
			// XXX spec unclear on 0s and negatives
			"100mm 100mm 100mm"
		]
	},
	"table-layout": {
		domProp: "tableLayout",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "fixed" ],
		invalid_values: []
	},
	"text-align": {
		domProp: "textAlign",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		// don't know whether left and right are same as start
		initial_values: [ "start" ],
		other_values: [ "center", "justify", "end" ],
		invalid_values: []
	},
	"-moz-text-blink": {
		domProp: "MozTextBlink",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "blink" ],
		invalid_values: [ "underline", "overline", "line-through", "none underline", "underline blink", "blink underline" ]
	},
	"text-decoration": {
		domProp: "textDecoration",
		inherited: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		subproperties: [ "-moz-text-blink", "-moz-text-decoration-color", "-moz-text-decoration-line", "-moz-text-decoration-style" ],
		initial_values: [ "none" ],
		other_values: [ "underline", "overline", "line-through", "blink", "blink line-through underline", "underline overline line-through blink", "-moz-anchor-decoration", "blink -moz-anchor-decoration" ],
		invalid_values: [ "none none", "underline none", "none underline", "blink none", "none blink", "line-through blink line-through", "underline overline line-through blink none", "underline overline line-throuh blink blink",
		                  "underline red solid", "underline #ff0000", "solid underline", "red underline", "#ff0000 underline" ]
	},
	"-moz-text-decoration-color": {
		domProp: "MozTextDecorationColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "black" },
		initial_values: [ "currentColor", "-moz-use-text-color" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"-moz-text-decoration-line": {
		domProp: "MozTextDecorationLine",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "underline", "overline", "line-through", "line-through underline", "underline overline line-through", "-moz-anchor-decoration", "-moz-anchor-decoration" ],
		invalid_values: [ "none none", "underline none", "none underline", "line-through blink line-through", "underline overline line-through blink none", "underline overline line-throuh blink blink" ]
	},
	"-moz-text-decoration-style": {
		domProp: "MozTextDecorationStyle",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "solid" ],
		other_values: [ "double", "dotted", "dashed", "wavy", "-moz-none" ],
		invalid_values: [ "none", "groove", "ridge", "inset", "outset", "solid dashed", "wave" ]
	},
	"text-indent": {
		domProp: "textIndent",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "-moz-calc(3em - 5em + 2px + 2em - 2px)" ],
		other_values: [ "2em", "5%", "-10px",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"text-overflow": {
		domProp: "textOverflow",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "clip", "clip clip" ],
		other_values: [ "ellipsis", '""', "''", '"hello"', 'clip ellipsis', 'clip ""', '"hello" ""', '"" ellipsis' ],
		invalid_values: [ "none", "auto", '"hello" inherit', 'inherit "hello"', 'clip initial', 'initial clip', 'initial inherit', 'inherit initial', 'inherit none']
	},
	"text-shadow": {
		domProp: "textShadow",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "blue" },
		initial_values: [ "none" ],
		other_values: [ "2px 2px", "2px 2px 1px", "2px 2px green", "2px 2px 1px green", "green 2px 2px", "green 2px 2px 1px", "green 2px 2px, blue 1px 3px 4px", "currentColor 3px 3px", "blue 2px 2px, currentColor 1px 2px",
			/* calc() values */
			"2px 2px -moz-calc(-5px)", /* clamped */
			"-moz-calc(3em - 2px) 2px green",
			"green -moz-calc(3em - 2px) 2px",
			"2px -moz-calc(2px + 0.2em)",
			"blue 2px -moz-calc(2px + 0.2em)",
			"2px -moz-calc(2px + 0.2em) blue",
			"-moz-calc(-2px) -moz-calc(-2px)",
			"-2px -2px",
			"-moz-calc(2px) -moz-calc(2px)",
			"-moz-calc(2px) -moz-calc(2px) -moz-calc(2px)",
		],
		invalid_values: [ "3% 3%", "2px 2px -5px", "2px 2px 2px 2px", "2px 2px, none", "none, 2px 2px", "inherit, 2px 2px", "2px 2px, inherit",
			"-moz-calc(2px) -moz-calc(2px) -moz-calc(2px) -moz-calc(2px)"
		]
	},
	"text-transform": {
		domProp: "textTransform",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "capitalize", "uppercase", "lowercase" ],
		invalid_values: []
	},
	"top": {
		domProp: "top",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* FIXME: run tests with multiple prerequisites */
		prerequisites: { "position": "relative" },
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"-moz-transition": {
		domProp: "MozTransition",
		inherited: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-transition-property", "-moz-transition-duration", "-moz-transition-timing-function", "-moz-transition-delay" ],
		initial_values: [ "all 0s ease 0s", "all", "0s", "0s 0s", "ease" ],
		other_values: [ "width 1s linear 2s", "width 1s 2s linear", "width linear 1s 2s", "linear width 1s 2s", "linear 1s width 2s", "linear 1s 2s width", "1s width linear 2s", "1s width 2s linear", "1s 2s width linear", "1s linear width 2s", "1s linear 2s width", "1s 2s linear width", "width linear 1s", "width 1s linear", "linear width 1s", "linear 1s width", "1s width linear", "1s linear width", "1s 2s width", "1s width 2s", "width 1s 2s", "1s 2s linear", "1s linear 2s", "linear 1s 2s", "width 1s", "1s width", "linear 1s", "1s linear", "1s 2s", "2s 1s", "width", "linear", "1s", "height", "2s", "ease-in-out", "2s ease-in", "opacity linear", "ease-out 2s", "2s color, 1s width, 500ms height linear, 1s opacity 4s cubic-bezier(0.0, 0.1, 1.0, 1.0)", "1s \\32width linear 2s", "1s -width linear 2s", "1s -\\32width linear 2s", "1s \\32 0width linear 2s", "1s -\\32 0width linear 2s", "1s \\2width linear 2s", "1s -\\2width linear 2s" ],
		invalid_values: [ "2s, 1s width", "1s width, 2s", "2s all, 1s width", "1s width, 2s all", "1s width, 2s none", "2s none, 1s width", "2s inherit", "inherit 2s", "2s width, 1s inherit", "2s inherit, 1s width", "2s initial", "2s all, 1s width", "2s width, 1s all" ]
	},
	"-moz-transition-delay": {
		domProp: "MozTransitionDelay",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0s", "0ms" ],
		other_values: [ "1s", "250ms", "-100ms", "-1s", "1s, 250ms, 2.3s"],
		invalid_values: [ "0", "0px" ]
	},
	"-moz-transition-duration": {
		domProp: "MozTransitionDuration",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0s", "0ms" ],
		other_values: [ "1s", "250ms", "-1ms", "-2s", "1s, 250ms, 2.3s"],
		invalid_values: [ "0", "0px" ]
	},
	"-moz-transition-property": {
		domProp: "MozTransitionProperty",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "all" ],
		other_values: [ "none", "left", "top", "color", "width, height, opacity", "foobar", "auto", "\\32width", "-width", "-\\32width", "\\32 0width", "-\\32 0width", "\\2width", "-\\2width" ],
		invalid_values: [ "none, none", "all, all", "color, none", "none, color", "all, color", "color, all", "inherit, color", "color, inherit", "initial, color", "color, initial", "none, color", "color, none", "all, color", "color, all" ]
	},
	"-moz-transition-timing-function": {
		domProp: "MozTransitionTimingFunction",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "ease", "cubic-bezier(0.25, 0.1, 0.25, 1.0)" ],
		other_values: [ "linear", "ease-in", "ease-out", "ease-in-out", "linear, ease-in, cubic-bezier(0.1, 0.2, 0.8, 0.9)", "cubic-bezier(0.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.25, 1.5, 0.75, -0.5)", "step-start", "step-end", "steps(1)", "steps(2, start)", "steps(386)", "steps(3, end)" ],
		invalid_values: [ "none", "auto", "cubic-bezier(0.25, 0.1, 0.25)", "cubic-bezier(0.25, 0.1, 0.25, 0.25, 1.0)", "cubic-bezier(-0.5, 0.5, 0.5, 0.5)", "cubic-bezier(1.5, 0.5, 0.5, 0.5)", "cubic-bezier(0.5, 0.5, -0.5, 0.5)", "cubic-bezier(0.5, 0.5, 1.5, 0.5)", "steps(2, step-end)", "steps(0)", "steps(-2)", "steps(0, step-end, 1)" ]
	},
	"unicode-bidi": {
		domProp: "unicodeBidi",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "embed", "bidi-override" ],
		invalid_values: [ "auto", "none" ]
	},
	"vertical-align": {
		domProp: "verticalAlign",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "baseline" ],
		other_values: [ "sub", "super", "top", "text-top", "middle", "bottom", "text-bottom", "15%", "3px", "0.2em", "-5px", "-3%",
			"-moz-calc(2px)",
			"-moz-calc(-2px)",
			"-moz-calc(50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(3*25px + 50%)",
		],
		invalid_values: []
	},
	"visibility": {
		domProp: "visibility",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "visible" ],
		other_values: [ "hidden", "collapse" ],
		invalid_values: []
	},
	"white-space": {
		domProp: "whiteSpace",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "pre", "nowrap", "pre-wrap", "pre-line" ],
		invalid_values: []
	},
	"widows": {
		domProp: "widows",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		// XXX requires display:block
		initial_values: [ "2" ],
		other_values: [ "1", "7" ],
		invalid_values: [ "0", "-1", "0px", "3px" ]
	},
	"width": {
		domProp: "width",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* computed value tests for width test more with display:block */
		prerequisites: { "display": "block" },
		initial_values: [ " auto" ],
		/* XXX these have prerequisites */
		other_values: [ "15px", "3em", "15%", "-moz-max-content", "-moz-min-content", "-moz-fit-content", "-moz-available",
			/* valid calc() values */
			"-moz-calc(-2px)",
			"-moz-calc(2px)",
			"-moz-calc(50%)",
			"-moz-calc(50% + 2px)",
			"-moz-calc( 50% + 2px)",
			"-moz-calc(50% + 2px )",
			"-moz-calc( 50% + 2px )",
			"-moz-calc(50% - -2px)",
			"-moz-calc(2px - -50%)",
			"-moz-calc(3*25px)",
			"-moz-calc(3 *25px)",
			"-moz-calc(3 * 25px)",
			"-moz-calc(3* 25px)",
			"-moz-calc(25px*3)",
			"-moz-calc(25px *3)",
			"-moz-calc(25px* 3)",
			"-moz-calc(25px * 3)",
			"-moz-calc(3*25px + 50%)",
			"-moz-calc(50% - 3em + 2px)",
			"-moz-calc(50% - (3em + 2px))",
			"-moz-calc((50% - 3em) + 2px)",
			"-moz-calc(2em)",
			"-moz-calc(50%)",
			"-moz-calc(50px/2)",
			"-moz-calc(50px/(2 - 1))"
		],
		invalid_values: [ "none", "-2px",
			/* invalid calc() values */
			"-moz-calc(50%+ 2px)",
			"-moz-calc(50% +2px)",
			"-moz-calc(50%+2px)",
			"-moz-min()",
			"-moz-calc(min())",
			"-moz-max()",
			"-moz-calc(max())",
			"-moz-min(5px)",
			"-moz-calc(min(5px))",
			"-moz-max(5px)",
			"-moz-calc(max(5px))",
			"-moz-min(5px,2em)",
			"-moz-calc(min(5px,2em))",
			"-moz-max(5px,2em)",
			"-moz-calc(max(5px,2em))",
			"-moz-calc(50px/(2 - 2))",
			/* If we ever support division by values, which is
			 * complicated for the reasons described in
			 * http://lists.w3.org/Archives/Public/www-style/2010Jan/0007.html
			 * , we should support all 4 of these as described in
			 * http://lists.w3.org/Archives/Public/www-style/2009Dec/0296.html
			 */
			"-moz-calc((3em / 100%) * 3em)",
			"-moz-calc(3em / 100% * 3em)",
			"-moz-calc(3em * (3em / 100%))",
			"-moz-calc(3em * 3em / 100%)"
		]
	},
	"word-spacing": {
		domProp: "wordSpacing",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal", "0", "0px", "-0em",
			"-moz-calc(-0px)", "-moz-calc(0em)"
		],
		other_values: [ "1em", "2px", "-3px",
			"-moz-calc(1em)", "-moz-calc(1em + 3px)",
			"-moz-calc(15px / 2)", "-moz-calc(15px/2)",
			"-moz-calc(-2em)"
		],
		invalid_values: []
	},
	"word-wrap": {
		domProp: "wordWrap",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "break-word" ],
		invalid_values: []
	},
	"-moz-hyphens": {
		domProp: "MozHyphens",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "manual" ],
		other_values: [ "none", "auto" ],
		invalid_values: []
	},
	"z-index": {
		domProp: "zIndex",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX requires position */
		initial_values: [ "auto" ],
		other_values: [ "0", "3", "-7000", "12000" ],
		invalid_values: [ "3.0", "17.5" ]
	}
	,
	"clip-path": {
		domProp: "clipPath",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#mypath)", "url('404.svg#mypath')" ],
		invalid_values: []
	},
	"clip-rule": {
		domProp: "clipRule",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "nonzero" ],
		other_values: [ "evenodd" ],
		invalid_values: []
	},
	"color-interpolation": {
		domProp: "colorInterpolation",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "sRGB" ],
		other_values: [ "auto", "linearRGB" ],
		invalid_values: []
	},
	"color-interpolation-filters": {
		domProp: "colorInterpolationFilters",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "linearRGB" ],
		other_values: [ "sRGB", "auto" ],
		invalid_values: []
	},
	"dominant-baseline": {
		domProp: "dominantBaseline",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "use-script", "no-change", "reset-size", "ideographic", "alphabetic", "hanging", "mathematical", "central", "middle", "text-after-edge", "text-before-edge" ],
		invalid_values: []
	},
	"fill": {
		domProp: "fill",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "blue" },
		initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
		other_values: [ "green", "#fc3", "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "none", "currentColor" ],
		invalid_values: []
	},
	"fill-opacity": {
		domProp: "fillOpacity",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1", "2.8", "1.000" ],
		other_values: [ "0", "0.3", "-7.3" ],
		invalid_values: []
	},
	"fill-rule": {
		domProp: "fillRule",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "nonzero" ],
		other_values: [ "evenodd" ],
		invalid_values: []
	},
	"filter": {
		domProp: "filter",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#myfilt)" ],
		invalid_values: [ "url(#myfilt) none" ]
	},
	"flood-color": {
		domProp: "floodColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "blue" },
		initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
		other_values: [ "green", "#fc3", "currentColor" ],
		invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green' ]
	},
	"flood-opacity": {
		domProp: "floodOpacity",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1", "2.8", "1.000" ],
		other_values: [ "0", "0.3", "-7.3" ],
		invalid_values: []
	},
	"image-rendering": {
		domProp: "imageRendering",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "optimizeSpeed", "optimizeQuality", "-moz-crisp-edges" ],
		invalid_values: []
	},
	"lighting-color": {
		domProp: "lightingColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "blue" },
		initial_values: [ "white", "#fff", "#ffffff", "rgb(255,255,255)", "rgba(255,255,255,1.0)", "rgba(255,255,255,42.0)" ],
		other_values: [ "green", "#fc3", "currentColor" ],
		invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green' ]
	},
	"marker": {
		domProp: "marker",
		inherited: true,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "marker-start", "marker-mid", "marker-end" ],
		initial_values: [ "none" ],
		other_values: [ "url(#mysym)" ],
		invalid_values: [ "none none", "url(#mysym) url(#mysym)", "none url(#mysym)", "url(#mysym) none" ]
	},
	"marker-end": {
		domProp: "markerEnd",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#mysym)" ],
		invalid_values: []
	},
	"marker-mid": {
		domProp: "markerMid",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#mysym)" ],
		invalid_values: []
	},
	"marker-start": {
		domProp: "markerStart",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#mysym)" ],
		invalid_values: []
	},
	"mask": {
		domProp: "mask",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(#mymask)" ],
		invalid_values: []
	},
	"shape-rendering": {
		domProp: "shapeRendering",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "optimizeSpeed", "crispEdges", "geometricPrecision" ],
		invalid_values: []
	},
	"stop-color": {
		domProp: "stopColor",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		prerequisites: { "color": "blue" },
		initial_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)" ],
		other_values: [ "green", "#fc3", "currentColor" ],
		invalid_values: [ "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green' ]
	},
	"stop-opacity": {
		domProp: "stopOpacity",
		inherited: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1", "2.8", "1.000" ],
		other_values: [ "0", "0.3", "-7.3" ],
		invalid_values: []
	},
	"stroke": {
		domProp: "stroke",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "black", "#000", "#000000", "rgb(0,0,0)", "rgba(0,0,0,1)", "green", "#fc3", "url('#myserver')", "url(foo.svg#myserver)", 'url("#myserver") green', "currentColor" ],
		invalid_values: []
	},
	"stroke-dasharray": {
		domProp: "strokeDasharray",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "5px,3px,2px", "5px 3px 2px", "  5px ,3px	, 2px ", "1px", "5%", "3em" ],
		invalid_values: [ "-5px,3px,2px", "5px,3px,-2px" ]
	},
	"stroke-dashoffset": {
		domProp: "strokeDashoffset",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "-0px", "0em" ],
		other_values: [ "3px", "3%", "1em" ],
		invalid_values: []
	},
	"stroke-linecap": {
		domProp: "strokeLinecap",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "butt" ],
		other_values: [ "round", "square" ],
		invalid_values: []
	},
	"stroke-linejoin": {
		domProp: "strokeLinejoin",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "miter" ],
		other_values: [ "round", "bevel" ],
		invalid_values: []
	},
	"stroke-miterlimit": {
		domProp: "strokeMiterlimit",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "4" ],
		other_values: [ "1", "7", "5000", "1.1" ],
		invalid_values: [ "0.9", "0", "-1", "3px", "-0.3" ]
	},
	"stroke-opacity": {
		domProp: "strokeOpacity",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1", "2.8", "1.000" ],
		other_values: [ "0", "0.3", "-7.3" ],
		invalid_values: []
	},
	"stroke-width": {
		domProp: "strokeWidth",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1px" ],
		other_values: [ "0", "0px", "-0em", "17px", "0.2em" ],
		invalid_values: [ "-0.1px", "-3px" ]
	},
	"text-anchor": {
		domProp: "textAnchor",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "start" ],
		other_values: [ "middle", "end" ],
		invalid_values: []
	},
	"text-rendering": {
		domProp: "textRendering",
		inherited: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "optimizeSpeed", "optimizeLegibility", "geometricPrecision" ],
		invalid_values: []
	}
}

function logical_box_prop_get_computed(cs, property)
{
	if (! /^-moz-/.test(property))
		throw "Unexpected property";
	property = property.substring(5);
	if (cs.getPropertyValue("direction") == "ltr")
		property = property.replace("-start", "-left").replace("-end", "-right");
	else
		property = property.replace("-start", "-right").replace("-end", "-left");
	return cs.getPropertyValue(property);
}

// Get the computed value for a property.  For shorthands, return the
// computed values of all the subproperties, delimited by " ; ".
function get_computed_value(cs, property)
{
	var info = gCSSProperties[property];
	if ("subproperties" in info) {
		var results = [];
		for (var idx in info.subproperties) {
			var subprop = info.subproperties[idx];
			results.push(get_computed_value(cs, subprop));
		}
		return results.join(" ; ");
	}
	if (info.get_computed)
		return info.get_computed(cs, property);
	return cs.getPropertyValue(property);
}
