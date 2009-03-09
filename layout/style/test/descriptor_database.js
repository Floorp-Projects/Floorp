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

// Each property has the following fields:
//   domProp: The name of the relevant member of nsIDOM[NS]CSS2Properties
//   values: Strings that are values for the descriptor and should be accepted.
//   invalid_values: Things that are not values for the descriptor and
//     should be rejected.

var gCSSFontFaceDescriptors = {
	"font-family": {
		domProp: "fontFamily",
		values: [ "\"serif\"", "\"cursive\"", "seriff", "Times New     Roman", "TimesRoman", "\"Times New Roman\"" ],
		/* not clear that the generics are really invalid */
		invalid_values: [ "sans-serif", "Times New Roman, serif", "'Times New Roman', serif", "cursive", "fantasy" ]
	},
	"font-stretch": {
		domProp: "fontStretch",
		values: [ "normal", "ultra-condensed", "extra-condensed", "condensed", "semi-condensed", "semi-expanded", "expanded", "extra-expanded", "ultra-expanded" ],
		invalid_values: [ "wider", "narrower" ]
	},
	"font-style": {
		domProp: "fontStyle",
		values: [ "normal", "italic", "oblique" ],
		invalid_values: []
	},
	"font-weight": {
		domProp: "fontWeight",
		values: [ "normal", "400", "bold", "100", "200", "300", "500", "600", "700", "800", "900" ],
		invalid_values: [ "107", "399", "401", "699", "710", "bolder", "lighter" ]
	},
	"src": {
		domProp: null,
		values: [
			"url(404.ttf)",
			"url(\"404.eot\")",
			"url(\'404.otf\')",
			"url(404.ttf) format(\"truetype\")",
			"url(404.ttf) format(\"truetype\", \"opentype\")",
			"url(404.ttf) format(\"truetype\", \"opentype\"), url(\'404.eot\')",
			"local(Times New Roman)",
			"local(\'Times New Roman\')",
			"local(\"Times New Roman\")",
			"local(serif)", /* is this valid? */
			"local(\"serif\")",
			"url(404.ttf) format(\"truetype\", \"unknown\"), local(Times New Roman), url(\'404.eot\')",
		],
		invalid_values: [
			"url(404.ttf) format(truetype)",
			"url(404.ttf) format(\"truetype\" \"opentype\")",
			"url(404.ttf) format(\"truetype\",)",
			"local(\"Times New\" Roman)",
		]
	},
	"unicode-range": {
		domProp: null,
		values: [ "U+0-10FFFF", "U+3-7B3", "U+3??", "U+6A", "U+3????", "U+???", "U+302-302", "U+0-7,A-C", "U+100-17F,200-17F", "U+3??, U+500-513 ,U+612 , U+4????", "U+1FFF,U+200-27F" ],
		invalid_values: [ "U+1????-2????" ]
	}
}
