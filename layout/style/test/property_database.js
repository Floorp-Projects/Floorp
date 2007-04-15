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

var gCSSProperties = {
	"-moz-appearance": {
		domProp: "MozAppearance",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "radio", "menulist" ],
		invalid_values: []
	},
	"-moz-background-clip": {
		domProp: "MozBackgroundClip",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "border" ],
		other_values: [ "padding" ],
		invalid_values: [ "content", "margin" ]
	},
	"-moz-background-inline-policy": {
		domProp: "MozBackgroundInlinePolicy",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "continuous" ],
		other_values: ["bounding-box", "each-box" ],
		invalid_values: []
	},
	"-moz-background-origin": {
		domProp: "MozBackgroundOrigin",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "padding" ],
		other_values: [ "border", "content" ],
		invalid_values: [ "margin" ]
	},
	"-moz-binding": {
		domProp: "MozBinding",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(foo.xml)" ],
		invalid_values: []
	},
	"-moz-border-bottom-colors": {
		domProp: "MozBorderBottomColors",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red, green", "red, #fc3", "#ff00cc" ],
		invalid_values: [ "red, none", "red, inherit" ]
	},
	"-moz-border-left-colors": {
		domProp: "MozBorderLeftColors",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red, green", "red, #fc3", "#ff00cc" ],
		invalid_values: [ "red, none", "red, inherit" ]
	},
	"-moz-border-radius": {
		domProp: "MozBorderRadius",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-border-radius-bottomleft", "-moz-border-radius-bottomright", "-moz-border-radius-topleft", "-moz-border-radius-topright" ],
		initial_values: [ "0", "0px", "0px 0 0 0px" ], /* 0% ? */
		other_values: [ "3%", "1px", "2em", "3em 2px", "2pt 3% 4em", "2px 2px 2px 2px" ],
		invalid_values: []
	},
	"-moz-border-radius-bottomleft": {
		domProp: "MozBorderRadiusBottomleft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px" ], /* 0% ? */
		other_values: [ "3%", "1px", "2em" ],
		invalid_values: []
	},
	"-moz-border-radius-bottomright": {
		domProp: "MozBorderRadiusBottomright",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px" ], /* 0% ? */
		other_values: [ "3%", "1px", "2em" ],
		invalid_values: []
	},
	"-moz-border-radius-topleft": {
		domProp: "MozBorderRadiusTopleft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px" ], /* 0% ? */
		other_values: [ "3%", "1px", "2em" ],
		invalid_values: []
	},
	"-moz-border-radius-topright": {
		domProp: "MozBorderRadiusTopright",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px" ], /* 0% ? */
		other_values: [ "3%", "1px", "2em" ],
		invalid_values: []
	},
	"-moz-border-right-colors": {
		domProp: "MozBorderRightColors",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red, green", "red, #fc3", "#ff00cc" ],
		invalid_values: [ "red, none", "red, inherit" ]
	},
	"-moz-border-top-colors": {
		domProp: "MozBorderTopColors",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "red, green", "red, #fc3", "#ff00cc" ],
		invalid_values: [ "red, none", "red, inherit" ]
	},
	"-moz-box-align": {
		domProp: "MozBoxAlign",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "stretch" ],
		other_values: [ "start", "center", "baseline", "end" ],
		invalid_values: []
	},
	"-moz-box-direction": {
		domProp: "MozBoxDirection",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "reverse" ],
		invalid_values: []
	},
	"-moz-box-flex": {
		domProp: "MozBoxFlex",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0.0", "-0.0" ],
		other_values: [ "1", "100", "0.1" ],
		invalid_values: [ "10px", "-1" ]
	},
	"-moz-box-ordinal-group": {
		domProp: "MozBoxOrdinalGroup",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "1" ],
		other_values: [ "0", "-1", "100", "-1000" ],
		invalid_values: [ "1.0" ]
	},
	"-moz-box-orient": {
		domProp: "MozBoxOrient",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "horizontal", "inline-axis" ],
		other_values: [ "vertical", "block-axis" ],
		invalid_values: []
	},
	"-moz-box-pack": {
		domProp: "MozBoxPack",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "start" ],
		other_values: [ "center", "end", "justify" ],
		invalid_values: []
	},
	"-moz-box-sizing": {
		domProp: "MozBoxSizing",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "content" ],
		other_values: [ "border", "padding" ],
		invalid_values: [ "margin" ]
	},
	"-moz-column-count": {
		domProp: "MozColumnCount",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "1", "0", "17" ],
		invalid_values: [
			// "-1", unclear: see http://lists.w3.org/Archives/Public/www-style/2007Apr/0030
			"3px"
		]
	},
	"-moz-column-gap": {
		domProp: "MozColumnGap",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal", "1em" ],
		other_values: [ "2px", "4em", "3%" ],
		invalid_values: []
	},
	"-moz-column-width": {
		domProp: "MozColumnWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "50%", "15px" ],
		invalid_values: [ "20" ]
	},
	"-moz-float-edge": {
		domProp: "MozFloatEdge",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "content" ],
		other_values: [ "border", "padding", "margin" ],
		invalid_values: []
	},
	"-moz-force-broken-image-icon": {
		domProp: "MozForceBrokenImageIcon",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0" ],
		other_values: [ "1" ],
		invalid_values: []
	},
	"-moz-image-region": {
		domProp: "MozImageRegion",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "rect(3px 20px 15px 4px)", "rect(17px, 21px, 33px, 2px)" ],
		invalid_values: []
	},
	"-moz-margin-end": {
		domProp: "MozMarginEnd",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* auto may or may not be initial */
		initial_values: [ "0", "0px", "0%", "0em", "0ex" ],
		other_values: [ "1px", "3em" ],
		invalid_values: []
	},
	"-moz-margin-start": {
		domProp: "MozMarginStart",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* auto may or may not be initial */
		initial_values: [ "0", "0px", "0%", "0em", "0ex" ],
		other_values: [ "1px", "3em" ],
		invalid_values: []
	},
	"-moz-outline-radius": {
		domProp: "MozOutlineRadius",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "-moz-outline-radius-bottomleft", "-moz-outline-radius-bottomright", "-moz-outline-radius-topleft", "-moz-outline-radius-topright" ],
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "2px", "0.3em", "2%" ],
		invalid_values: []
	},
	"-moz-outline-radius-bottomleft": {
		domProp: "MozOutlineRadiusBottomleft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "2px", "0.3em", "2%" ],
		invalid_values: []
	},
	"-moz-outline-radius-bottomright": {
		domProp: "MozOutlineRadiusBottomright",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "2px", "0.3em", "2%" ],
		invalid_values: []
	},
	"-moz-outline-radius-topleft": {
		domProp: "MozOutlineRadiusTopleft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "2px", "0.3em", "2%" ],
		invalid_values: []
	},
	"-moz-outline-radius-topright": {
		domProp: "MozOutlineRadiusTopright",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "2px", "0.3em", "2%" ],
		invalid_values: []
	},
	"-moz-padding-end": {
		domProp: "MozPaddingEnd",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "0em", "0ex" ],
		other_values: [ "1px", "3em" ],
		invalid_values: []
	},
	"-moz-padding-start": {
		domProp: "MozPaddingStart",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [ "0", "0px", "0%", "0em", "0ex" ],
		other_values: [ "1px", "3em" ],
		invalid_values: []
	},
	"-moz-user-focus": {
		domProp: "MozUserFocus",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "normal", "ignore", "select-all", "select-before", "select-after", "select-same", "select-menu" ],
		invalid_values: []
	},
	"-moz-user-input": {
		domProp: "MozUserInput",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "none", "enabled", "disabled" ],
		invalid_values: []
	},
	"-moz-user-modify": {
		domProp: "MozUserModify",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "read-only" ],
		other_values: [ "read-write", "write-only" ],
		invalid_values: []
	},
	"-moz-user-select": {
		domProp: "MozUserSelect",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "none", "text", "element", "elements", "all", "toggle", "tri-state", "-moz-all", "-moz-none" ],
		invalid_values: []
	},
	"azimuth": {
		domProp: "azimuth",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "center", "0deg" ],
		other_values: [ "center behind", "behind far-right", "left-side", "73deg", "90.1deg", "0.1deg" ],
		invalid_values: [ "0deg behind", "behind 0deg", "90deg behind", "behind 90deg" ]
	},
	"background": {
		domProp: "background",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "background-attachment", "background-color", "background-image", "background-position", "background-repeat", "-moz-background-clip", "-moz-background-inline-policy", "-moz-background-origin" ],
		initial_values: [ "transparent", "none", "repeat", "scroll", "0% 0%", "top left", "left top", "transparent none", "top left none", "left top none", "none left top", "none top left", "none 0% 0%", "transparent none repeat scroll top left", "left top repeat none scroll transparent"],
		other_values: [ "green", "none green repeat scroll left top", "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "repeat url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==') transparent left top scroll", "repeat-x", "repeat-y", "no-repeat", "none repeat-y transparent scroll 0% 0%", "fixed", "0% top transparent fixed repeat none", "top", "left", "50% 50%", "center", "bottom right scroll none transparent repeat", "50% transparent", "transparent 50%", "50%" ],
 		invalid_values: [
 			/* mixes with keywords have to be in correct order */
 			"50% left", "top 50%",
 			/* bug 258080: don't accept background-position separated */
 			"left url(404.png) top", "top url(404.png) left"
 		]
	},
	"background-attachment": {
		domProp: "backgroundAttachment",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "scroll" ],
		other_values: [ "fixed" ],
		invalid_values: []
	},
	"background-color": {
		domProp: "backgroundColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "transparent", "rgba(255, 127, 15, 0)", "hsla(240, 97%, 50%, 0.0)", "rgba(0, 0, 0, 0)", "rgba(255,255,255,-3.7)" ],
		other_values: [ "green", "rgb(255, 0, 128)", "rgb(255.0,0.387,3489)", "#fc2", "#96ed2a", "black", "rgba(255,255,0,3)" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"background-image": {
		domProp: "backgroundImage",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")', ],
		invalid_values: []
	},
	"background-position": {
		domProp: "backgroundPosition",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "top left", "left top", "0% 0%", "0% top", "left 0%" ],
		other_values: [ "top", "left", "right", "bottom", "center", "center bottom", "bottom center", "center right", "right center", "center top", "top center", "center left", "left center", "right bottom", "bottom right", "50%" ],
		invalid_values: [ "50% left", "top 50%" ]
	},
	"background-repeat": {
		domProp: "backgroundRepeat",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "repeat" ],
		other_values: [ "repeat-x", "repeat-y", "no-repeat" ],
		invalid_values: []
	},
	"border": {
		domProp: "border",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width", "border-left-color", "border-left-style", "border-left-width", "border-right-color", "border-right-style", "border-right-width", "border-top-color", "border-top-style", "border-top-width" ],
		initial_values: [ "none", "medium", "currentColor", "none medium currentcolor" ],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"border-bottom": {
		domProp: "borderBottom",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-bottom-color", "border-bottom-style", "border-bottom-width" ],
		initial_values: [ "none", "medium", "currentColor", "none medium currentcolor" ],
		other_values: [ "solid", "thin", "green", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"border-bottom-color": {
		domProp: "borderBottomColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-bottom-style": {
		domProp: "borderBottomStyle",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-bottom-width": {
		domProp: "borderBottomWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX a value differing from initial requires border-style to be set */
		initial_values: [ "medium", "3px" ],
		other_values: [ "thin", "thick", "1px", "2em" ],
		invalid_values: [ "5%" ]
	},
	"border-collapse": {
		domProp: "borderCollapse",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "separate" ],
		other_values: [ "collapse" ],
		invalid_values: []
	},
	"border-color": {
		domProp: "borderColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-color", "border-right-color", "border-bottom-color", "border-left-color" ],
		initial_values: [ "currentColor", "currentColor currentColor", "currentColor currentColor currentColor", "currentColor currentColor currentcolor CURRENTcolor" ],
		other_values: [ "green", "currentColor green", "currentColor currentColor green", "currentColor currentColor currentColor green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-left": {
		domProp: "borderLeft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-left-color", "border-left-style", "border-left-width" ],
		initial_values: [ "none", "medium", "currentColor", "none medium currentcolor" ],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"border-left-color": {
		domProp: "borderLeftColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-left-style": {
		domProp: "borderLeftStyle",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-left-width": {
		domProp: "borderLeftWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX a value differing from initial requires border-style to be set */
		initial_values: [ "medium", "3px" ],
		other_values: [ "thin", "thick", "1px", "2em" ],
		invalid_values: [ "5%" ]
	},
	"border-right": {
		domProp: "borderRight",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-right-color", "border-right-style", "border-right-width" ],
		initial_values: [ "none", "medium", "currentColor", "none medium currentcolor" ],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"border-right-color": {
		domProp: "borderRightColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-right-style": {
		domProp: "borderRightStyle",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-right-width": {
		domProp: "borderRightWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX a value differing from initial requires border-style to be set */
		initial_values: [ "medium", "3px" ],
		other_values: [ "thin", "thick", "1px", "2em" ],
		invalid_values: [ "5%" ]
	},
	"border-spacing": {
		domProp: "borderSpacing",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "0", "0 0", "0px", "0 0px" ],
		other_values: [ "3px", "4em 2px", "4em 0", "0px 2px" ],
		invalid_values: [ "0%", "0 0%" ]
	},
	"border-style": {
		domProp: "borderStyle",
		inherited: false,
		backend_only: false,
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
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-color", "border-top-style", "border-top-width" ],
		initial_values: [ "none", "medium", "currentColor", "none medium currentcolor" ],
		other_values: [ "solid", "medium solid", "green solid", "10px solid", "thick solid" ],
		invalid_values: [ "5%" ]
	},
	"border-top-color": {
		domProp: "borderTopColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "currentColor" ],
		other_values: [ "green", "rgba(255,128,0,0.5)", "transparent" ],
		invalid_values: [ "#0", "#00", "#0000", "#00000", "#0000000", "#00000000", "#000000000" ]
	},
	"border-top-style": {
		domProp: "borderTopStyle",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX hidden is sometimes the same as initial */
		initial_values: [ "none" ],
		other_values: [ "solid", "dashed", "dotted", "double", "outset", "inset", "groove", "ridge" ],
		invalid_values: []
	},
	"border-top-width": {
		domProp: "borderTopWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX a value differing from initial requires border-style to be set */
		initial_values: [ "medium", "3px" ],
		other_values: [ "thin", "thick", "1px", "2em" ],
		invalid_values: [ "5%" ]
	},
	"border-width": {
		domProp: "borderWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "border-top-width", "border-right-width", "border-bottom-width", "border-left-width" ],
		/* XXX a value differing from initial requires border-style to be set */
		initial_values: [ "medium", "3px", "medium medium", "3px medium medium", "medium 3px medium medium" ],
		other_values: [ "thin", "thick", "1px", "2em", "2px 0 0px 1em" ],
		invalid_values: [ "5%" ]
	},
	"bottom": {
		domProp: "bottom",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX requires position to be set */
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%" ],
		invalid_values: []
	},
	"caption-side": {
		domProp: "captionSide",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "top" ],
		other_values: [ "right", "left", "bottom" ],
		invalid_values: []
	},
	"clear": {
		domProp: "clear",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "left", "right", "both" ],
		invalid_values: []
	},
	"clip": {
		domProp: "clip",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "rect(auto,auto,auto,auto)", "rect(3px, 4px, 4em, 0)", "rect(auto, 3em, 4pt, 2px)", "rect(2px 3px 4px 5px)" ],
		invalid_values: [ "rect(auto, 3em, 2%, 5px)" ]
	},
	"color": {
		domProp: "color",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "currentcolor" ],
		other_values: [ "green", "#f3c", "#fed292", "rgba(45,300,12,2)", "transparent" ],
		invalid_values: [ "fff", "ffffff", "#f", "#ff", "#ffff", "#fffff", "#fffffff", "#ffffffff", "#fffffffff" ]
	},
	"content": {
		domProp: "content",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX needs to be on pseudo-elements */
		initial_values: [ "normal", "none" ],
		other_values: [ '""', "''", '"hello"', "url(data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==)", "url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==')", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")', ],
		invalid_values: []
	},
	"counter-increment": {
		domProp: "counterIncrement",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "foo 1", "bar", "foo 3 bar baz 2" ],
		invalid_values: []
	},
	"counter-reset": {
		domProp: "counterReset",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "bar 0", "foo", "foo 3 bar baz 2" ],
		invalid_values: []
	},
	"cue": {
		domProp: "cue",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "cue-before", "cue-after" ],
		initial_values: [ "none", "none none" ],
		other_values: [ "url(404.wav)", "url(404.wav) none", "none url(404.wav)" ],
		invalid_values: []
	},
	"cue-after": {
		domProp: "cueAfter",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(404.wav)" ],
		invalid_values: []
	},
	"cue-before": {
		domProp: "cueBefore",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "url(404.wav)" ],
		invalid_values: []
	},
	"cursor": {
		domProp: "cursor",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "crosshair", "default", "pointer", "move", "e-resize", "ne-resize", "nw-resize", "n-resize", "se-resize", "sw-resize", "s-resize", "w-resize", "text", "wait", "help", "progress" ],
		invalid_values: []
	},
	"direction": {
		domProp: "direction",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "ltr" ],
		other_values: [ "rtl" ],
		invalid_values: []
	},
	"display": {
		domProp: "display",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "inline" ],
		/* XXX none will really mess with other properties */
		other_values: [ "block", "list-item", "inline-block", "table", "inline-table", "table-row-group", "table-header-group", "table-footer-group", "table-row", "table-column-group", "table-column", "table-cell", "table-caption", "none" ],
		invalid_values: []
	},
	"elevation": {
		domProp: "elevation",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "level", "0deg" ],
		other_values: [ "below", "above", "60deg", "higher", "lower", "-79deg", "0.33deg" ],
		invalid_values: []
	},
	"empty-cells": {
		domProp: "emptyCells",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "show" ],
		other_values: [ "hide" ],
		invalid_values: []
	},
	"float": {
		domProp: "cssFloat",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "left", "right" ],
		invalid_values: []
	},
	"font": {
		domProp: "font",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "font-style", "font-variant", "font-weight", "font-size", "line-height", "font-family", "font-stretch", "font-size-adjust" ],
		/* XXX could be sans-serif */
		initial_values: [ "medium serif" ],
		other_values: [ "large serif", "9px fantasy", "bold italic small-caps 24px/1.4 Times New Roman, serif", "caption", "icon", "menu", "message-box", "small-caption", "status-bar" ],
		invalid_values: []
	},
	"font-family": {
		domProp: "fontFamily",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "serif" ],
		other_values: [ "sans-serif", "Times New Roman, serif", "'Times New Roman', serif", "cursive", "fantasy" ],
		invalid_values: []
	},
	"font-size": {
		domProp: "fontSize",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "medium" ],
		other_values: [ "2em", "50%", "large", "xx-small", "36pt", "8px" ],
		invalid_values: []
	},
	"font-size-adjust": {
		domProp: "fontSizeAdjust",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ "0.3", "0.5", "0.7" ],
		invalid_values: []
	},
	"font-stretch": {
		domProp: "fontStretch",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "wider", "narrower", "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded" ],
		invalid_values: []
	},
	"font-style": {
		domProp: "fontStyle",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "italic", "oblique" ],
		invalid_values: []
	},
	"font-variant": {
		domProp: "fontVariant",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "small-caps" ],
		invalid_values: []
	},
	"font-weight": {
		domProp: "fontWeight",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "bold", "100", "200", "300", "400", "500", "600", "700", "800", "900" ],
		invalid_values: [ "107", "399", "401", "699", "710" ]
	},
	"height": {
		domProp: "height",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ " auto" ],
		/* XXX these have prerequisites */
		other_values: [ "15px", "3em", "15%" ],
		invalid_values: []
	},
	"left": {
		domProp: "left",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX requires position to be set */
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%" ],
		invalid_values: []
	},
	"letter-spacing": {
		domProp: "letterSpacing",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "0", "0px", "1em", "2px", "-3px" ],
		invalid_values: []
	},
	"line-height": {
		domProp: "lineHeight",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "normal" ],
		other_values: [ "1.0", "1", "1em", "27px" ],
		invalid_values: []
	},
	"list-style": {
		domProp: "listStyle",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "list-style-type", "list-style-position", "list-style-image" ],
		initial_values: [ "none", "outside", "disc", "none disc outside" ],
		other_values: [ "inside none", "none inside", "none none inside", "none outside none", "square", 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")' ],
		invalid_values: []
	},
	"list-style-image": {
		domProp: "listStyleImage",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "none" ],
		other_values: [ 'url("data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAIAAAD8GO2jAAAAKElEQVR42u3NQQ0AAAgEoNP+nTWFDzcoQE1udQQCgUAgEAgEAsGTYAGjxAE/G/Q2tQAAAABJRU5ErkJggg==")' ],
		invalid_values: []
	},
	"list-style-position": {
		domProp: "listStylePosition",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "outside" ],
		other_values: [ "inside" ],
		invalid_values: []
	},
	"list-style-type": {
		domProp: "listStyleType",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "disc" ],
		other_values: [ "circle", "decimal-leading-zero", "upper-alpha" ],
		invalid_values: []
	},
	"margin": {
		domProp: "margin",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "margin-top", "margin-right", "margin-bottom", "margin-left" ],
		initial_values: [ "0", "0px 0 0em", "0% 0px 0em 0pt" ],
		other_values: [ "3px 0", "2em 4px 2pt", "1em 2em 3px 4px" ],
		invalid_values: []
	},
	"margin-bottom": {
		domProp: "marginBottom",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "1px", "2em", "5%" ],
		invalid_values: [ ]
	},
	"margin-left": {
		domProp: "marginLeft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "1px", "2em", "5%" ],
		invalid_values: []
	},
	"margin-right": {
		domProp: "marginRight",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "1px", "2em", "5%" ],
		invalid_values: [ ]
	},
	"margin-top": {
		domProp: "marginTop",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX testing auto has prerequisites */
		initial_values: [ "0", "0px", "0%" ],
		other_values: [ "1px", "2em", "5%" ],
		invalid_values: [ ]
	},
	"marker-offset": {
		domProp: "markerOffset",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [ "auto" ],
		other_values: [ "6em", "-1px" ],
		invalid_values: []
	},
	"marks": {
		/* XXX not a real property; applies only to page context */
		domProp: "marks",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"max-height": {
		domProp: "maxHeight",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"max-width": {
		domProp: "maxWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"min-height": {
		domProp: "minHeight",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"min-width": {
		domProp: "minWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"opacity": {
		domProp: "opacity",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"orphans": {
		domProp: "orphans",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"outline": {
		domProp: "outline",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "outline-color", "outline-style", "outline-width" ],
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"outline-color": {
		domProp: "outlineColor",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"outline-offset": {
		domProp: "outlineOffset",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"outline-style": {
		domProp: "outlineStyle",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"outline-width": {
		domProp: "outlineWidth",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"overflow": {
		domProp: "overflow",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		subproperties: [ "overflow-x", "overflow-y" ],
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"overflow-x": {
		domProp: "overflowX",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"overflow-y": {
		domProp: "overflowY",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"padding": {
		domProp: "padding",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "padding-top", "padding-right", "padding-bottom", "padding-left" ],
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"padding-bottom": {
		domProp: "paddingBottom",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"padding-left": {
		domProp: "paddingLeft",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"padding-right": {
		domProp: "paddingRight",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_SHORTHAND_AND_LONGHAND,
		/* no subproperties */
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"padding-top": {
		domProp: "paddingTop",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"page": {
		domProp: "page",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"page-break-after": {
		domProp: "pageBreakAfter",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"page-break-before": {
		domProp: "pageBreakBefore",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"page-break-inside": {
		domProp: "pageBreakInside",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pause": {
		domProp: "pause",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "pause-before", "pause-after" ],
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pause-after": {
		domProp: "pauseAfter",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pause-before": {
		domProp: "pauseBefore",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pitch": {
		domProp: "pitch",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pitch-range": {
		domProp: "pitchRange",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"position": {
		domProp: "position",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"quotes": {
		domProp: "quotes",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"richness": {
		domProp: "richness",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"right": {
		domProp: "right",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX requires position to be set */
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%" ],
		invalid_values: []
	},
	"size": {
		/* XXX not a real property; applies only to page context */
		domProp: "size",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"speak": {
		domProp: "speak",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"speak-header": {
		domProp: "speakHeader",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"speak-numeral": {
		domProp: "speakNumeral",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"speak-punctuation": {
		domProp: "speakPunctuation",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"speech-rate": {
		domProp: "speechRate",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stress": {
		domProp: "stress",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"table-layout": {
		domProp: "tableLayout",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-align": {
		domProp: "textAlign",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-decoration": {
		domProp: "textDecoration",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-indent": {
		domProp: "textIndent",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-shadow": {
		domProp: "textShadow",
		inherited: false,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-transform": {
		domProp: "textTransform",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"top": {
		domProp: "top",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		/* XXX requires position to be set */
		/* XXX 0 may or may not be equal to auto */
		initial_values: [ "auto" ],
		other_values: [ "32px", "-3em", "12%" ],
		invalid_values: []
	},
	"unicode-bidi": {
		domProp: "unicodeBidi",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"vertical-align": {
		domProp: "verticalAlign",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"visibility": {
		domProp: "visibility",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"voice-family": {
		domProp: "voiceFamily",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"volume": {
		domProp: "volume",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"white-space": {
		domProp: "whiteSpace",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"widows": {
		domProp: "widows",
		inherited: true,
		backend_only: true,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"width": {
		domProp: "width",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"word-spacing": {
		domProp: "wordSpacing",
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"z-index": {
		domProp: "zIndex",
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	}
	,
	"clip-path": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"clip-rule": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"color-interpolation": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"color-interpolation-filters": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"dominant-baseline": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"fill": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"fill-opacity": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"fill-rule": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"filter": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"flood-color": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"flood-opacity": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"marker": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_TRUE_SHORTHAND,
		subproperties: [ "marker-start", "marker-mid", "marker-end" ],
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"marker-end": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"marker-mid": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"marker-start": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"mask": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"pointer-events": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"shape-rendering": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stop-color": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stop-opacity": {
		domProp: null,
		inherited: false,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-dasharray": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-dashoffset": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-linecap": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-linejoin": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-miterlimit": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-opacity": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"stroke-width": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-anchor": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	},
	"text-rendering": {
		domProp: null,
		inherited: true,
		backend_only: false,
		type: CSS_TYPE_LONGHAND,
		initial_values: [],
		other_values: [],
		invalid_values: []
	}
}
