/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Beth Epperson
 */

// HTML Attributes object for "Name" menulist
var gHTMLAttr = {};

// JS Events Attributes object for "Name" menulist
var gJSAttr = {};


// Core HTML attribute values //
// This is appended to Name menulist when "_core" is attribute name
var gCoreHTMLAttr =
[
  "^id",
	"^class",
	"title"
];

// Core event attribute values //
// This is appended to all JS menulists
//   except those elements having "noJSEvents"
//   as a value in their gJSAttr array.
var gCoreJSEvents =
[
	"onclick",
	"ondblclick",
	"onmousedown",
	"onmouseup",
	"onmouseover",
	"onmousemove",
	"onmouseout",
  "-",
  "onkeypress",
	"onkeydown",
	"onkeyup"
];

// Following are commonly-used strings

// Alse accept: sRGB: #RRGGBB //
var gHTMLColors =
[
	"Black",
	"Green",
	"Silver",
	"Lime",
	"Gray",
	"Olive",
	"White",
	"Yellow",
	"Maroon",
	"Navy",
	"Red",
	"Blue",
	"Purple",
	"Teal",
	"Fuchsia",
	"Aqua"
];

var gHAlign =
[
	"left",
	"center",
	"right"
];

var gHAlignJustify =
[
	"left",
	"center",
	"right",
	"justify"
];

var gHAlignTableContent =
[
	"left",
	"center",
	"right",
	"justify",
	"char"
];

var gVAlignTable =
[
	"top",
	"middle",
	"bottom",
	"baseline"
];

// ================ HTML Attributes ================ //
/* For each element, there is an array of attributes,
   whose name is the element name,
   used to fill the "Attribute Name" menulist.
   For each of those attributes, if they have a specific
   set of values, those are listed in an array named:
   "elementName_attName".

   In each values string, the following characters
   are signal to do input filtering:
    "#"  Allow only integer values
    "%"  Allow integer values or a number ending in "%"
    "+"  Allow integer values and allow "+" or "-" as first character
    "!"  Allow only one character
    "^"  The first character can be only be A-Z, a-z, hyphen, underscore, colon or period
    "$"  is an attribute required by HTML DTD
*/

/*
   Most elements have the "dir" attribute,
   so we use this value array
   for all elements instead of specifying  
   separately for each element
*/
gHTMLAttr.all_dir =
[
  "ltr",
  "rtl"
];


gHTMLAttr.a =
[
  "charset",
  "type",
  "name",
  "href",
  "^hreflang",
  "target",
  "rel",
  "rev",
  "!accesskey",
  "shape",		// with imagemap //
  "coords",		// with imagemap //
  "#tabindex",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.a_rel =
[
  "alternate",
  "stylesheet",
  "start",
  "next",
  "prev",
  "contents",
  "index",
  "glossary",
  "copyright",
  "chapter",
  "section",
  "subsection",
  "appendix",
  "help",
  "bookmark"
];

gHTMLAttr.a_rev =
[
  "alternate",
  "stylesheet",
  "start",
  "next",
  "prev",
  "contents",
  "index",
  "glossary",
  "copyright",
  "chapter",
  "section",
  "subsection",
  "appendix",
  "help",
  "bookmark"
];

gHTMLAttr.a_shape =
[
  "rect",
  "circle",
  "poly",
  "default"
];

gHTMLAttr.abbr =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.acronym =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.address =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

// this is deprecated //
gHTMLAttr.applet =
[
  "codebase",
  "archive",
  "code",
  "object",
  "alt",
  "name",
  "%$width",
  "%$height",
  "align",
  "#hspace",
  "#vspace",
  "-",
  "_core"
];

gHTMLAttr.applet_align =
[
  "top",
  "middle",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.area =
[
  "shape",
  "coords",
  "href",
  "nohref",
  "target",
  "$alt",
  "#tabindex",
  "!accesskey",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.area_shape =
[
  "rect",
  "circle",
  "poly",
  "default"
];

gHTMLAttr.area_nohref =
[
  "nohref"
];

gHTMLAttr.b =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.base =
[
  "href",
  "target"
];

// this is deprecated //
gHTMLAttr.basefont =
[
  "^id",
  "$size",
  "color",
  "face"
];
 
gHTMLAttr.basefont_color = gHTMLColors;

gHTMLAttr.bdo =
[
  "_core",
  "-",
  "^lang",
  "$dir"
];

gHTMLAttr.bdo_dir =
[
  "ltr",
  "rtl"
];

gHTMLAttr.big =
[
  "_core",
  "-",
  "^lang",
  "dir"
];
 
gHTMLAttr.blockquote =
[
  "cite",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.body =
[
  "background",
  "bgcolor",
  "text",
  "link",
  "vlink",
  "alink",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.body_bgcolor = gHTMLColors;

gHTMLAttr.body_text = gHTMLColors;

gHTMLAttr.body_link = gHTMLColors;

gHTMLAttr.body_vlink = gHTMLColors;

gHTMLAttr.body_alink = gHTMLColors;

gHTMLAttr.br =
[
  "clear",
  "-",
  "_core"
];

gHTMLAttr.br_clear =
[
  "none",
  "left",
  "all",
  "right"
];

gHTMLAttr.button =
[
  "name",
  "value",
  "$type",
  "disabled",
  "#tabindex",
  "!accesskey",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.button_type =
[
  "submit",
  "button",
  "reset"
];

gHTMLAttr.button_disabled =
[
  "disabled"
];

gHTMLAttr.caption =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.caption_align =
[
  "top",
  "bottom",
  "left",
  "right"
];


// this is deprecated //
gHTMLAttr.center =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.cite =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.code =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.col =
[
  "#$span",
  "%width",
  "align",
  "!char",
  "#charoff",
  "valign",
  "char",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.col_span =
[
  "1"  // default
];

gHTMLAttr.col_align = gHAlignTableContent;

gHTMLAttr.col_valign =
[
  "top",
  "middle",
  "bottom",
  "baseline"
];


gHTMLAttr.colgroup =
[
  "#$span",
  "%width",
  "align",
  "!char",
  "#charoff",
  "valign",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.colgroup_span =
[
  "1" // default
];

gHTMLAttr.colgroup_align = gHAlignTableContent;

gHTMLAttr.colgroup_valign =
[
  "top",
  "middle",
  "bottom",
  "baseline"
];

gHTMLAttr.dd =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.del =
[
  "cite",
  "datetime",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.dfn =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

// this is deprecated //
gHTMLAttr.dir =
[
  "compact",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.dir_compact =
[
  "compact"
];

gHTMLAttr.div =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.div_align = gHAlignJustify;

gHTMLAttr.dl =
[
  "compact",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.dl_compact =
[
  "compact"
];


gHTMLAttr.dt =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.em =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.fieldset =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

// this is deprecated //
gHTMLAttr.font =
[
  "+size",
  "color",
  "face",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.font_color = gHTMLColors;

gHTMLAttr.form =
[
  "$action",
  "$method",
  "enctype",
  "accept",
  "name",
  "accept-charset",
  "target",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.form_method =
[
  "get",
  "post"
];

gHTMLAttr.form_enctype =
[
  "application/x-www-form-urlencoded"
];

gHTMLAttr.form_target =
[
  "blank",
  "self",
  "parent",
  "top"
];

gHTMLAttr.frame =
[
  "longdesc",
  "name",
  "src",
  "#frameborder",
  "#marginwidth",
  "#marginheight",
  "noresize",
  "$scrolling"
];

gHTMLAttr.frame_frameborder =
[
  "1",
  "0"
];

gHTMLAttr.frame_noresize =
[
  "noresize"
];

gHTMLAttr.frame_scrolling =
[
  "auto",
  "yes",
  "no"
];


gHTMLAttr.frameset =
[
  "rows",
  "cols",
  "-",
  "_core"
];

gHTMLAttr.h1 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h1_align = gHAlignJustify;

gHTMLAttr.h2 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h2_align = gHAlignJustify;

gHTMLAttr.h3 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h3_align =  gHAlignJustify;

gHTMLAttr.h4 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h4_align = gHAlignJustify;


gHTMLAttr.h5 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h5_align = gHAlignJustify;

gHTMLAttr.h6 =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.h6_align = gHAlignJustify;

gHTMLAttr.head =
[
  "profile",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.hr =
[
  "align",
  "noshade",
  "#size",
  "%width",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.hr_align = gHAlign;

gHTMLAttr.hr_noshade = 
[
  "noshade"
];


gHTMLAttr.html =
[
  "version",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.i =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.iframe =
[
  "longdesc",
  "name",
  "src",
  "$frameborder",
  "marginwidth",
  "marginheight",
  "$scrolling",
  "align",
  "%height",
  "%width",
  "-",
  "_core"
];

gHTMLAttr.iframe_frameborder =
[
  "1",
  "0"
];

gHTMLAttr.iframe_scrolling =
[
  "auto",
  "yes",
  "no"
];

gHTMLAttr.iframe_align =
[
  "top",
  "middle",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.img =
[
  "$src",
  "$alt",
  "longdesc",
  "name",
  "%height",
  "%width",
  "usemap",
  "ismap",
  "align",
  "#border",
  "#hspace",
  "#vspace",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.img_ismap =
[
  "ismap"
];

gHTMLAttr.img_align =
[
  "top",
  "middle",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.input =
[
  "$type",
  "name",
  "value",
  "checked",
  "disabled",
  "readonly",
  "#size",
  "#maxlength",
  "src",
  "alt",
  "usemap",
  "ismap",
  "#tabindex",
  "!accesskey",
  "accept",
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.input_type =
[
  "text",
  "password",
  "checkbox",
  "radio",
  "submit",
  "reset",
  "file",
  "hidden",
  "image",
  "button"
];

gHTMLAttr.input_checked =
[
  "checked"
];

gHTMLAttr.input_disabled =
[
  "disabled"
];

gHTMLAttr.input_readonly =
[
  "readonly"
];


gHTMLAttr.input_ismap =
[
  "ismap"
];


gHTMLAttr.input_align =
[
  "top",
  "middle",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.ins =
[
  "cite",
  "datetime",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.isindex =
[
  "prompt",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.kbd =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.label =
[
  "for",
  "!accesskey",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.legend =
[
  "!accesskey",
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.legend_align =
[
  "top",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.li =
[
  "type",
  "#value",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.li_type =
[
  "disc",
  "square",
  "circle",
  "-",
  "1",
  "a",
  "A",
  "i",
  "I"
];

gHTMLAttr.link =
[
  "charset",
  "href",
  "^hreflang",
  "type",
  "rel",
  "rev",
  "media",
  "target",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.link_rel =
[
  "alternate",
  "stylesheet",
  "start",
  "next",
  "prev",
  "contents",
  "index",
  "glossary",
  "copyright",
  "chapter",
  "section",
  "subsection",
  "appendix",
  "help",
  "bookmark"
];

gHTMLAttr.link_rev =
[
  "alternate",
  "stylesheet",
  "start",
  "next",
  "prev",
  "contents",
  "index",
  "glossary",
  "copyright",
  "chapter",
  "section",
  "subsection",
  "appendix",
  "help",
  "bookmark"
];

gHTMLAttr.map =
[
  "$name",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.menu =
[
  "compact",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.menu_compact =
[
  "compact"
];

gHTMLAttr.meta =
[
  "http-equiv",
  "name",
  "$content",
  "scheme",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.noframes =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.noscript =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.object =
[
  "declare",
  "classid",
  "codebase",
  "data",
  "type",
  "codetype",
  "archive",
  "standby",
  "%height",
  "%width",
  "usemap",
  "name",
  "#tabindex",
  "align",
  "#border",
  "#hspace",
  "#vspace",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.object_declare =
[
  "declare"
];

gHTMLAttr.object_align =
[
  "top",
  "middle",
  "bottom",
  "left",
  "right"
];

gHTMLAttr.ol =
[
  "type",
  "compact",
  "#start",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.ol_type =
[
  "1",
  "a",
  "A",
  "i",     
  "I"
];

gHTMLAttr.ol_compact =
[
  "compact"
];


gHTMLAttr.optgroup =
[
  "disabled",
  "$label",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.optgroup_disabled =
[
  "disabled"
];


gHTMLAttr.option =
[
  "selected",
  "disabled",
  "label",
  "value",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.option_selected =
[
  "selected"
];

gHTMLAttr.option_disabled =
[
  "disabled"
];


gHTMLAttr.p =
[
  "align",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.p_align = gHAlignJustify;

gHTMLAttr.param =
[
  "^id",
  "$name",
  "value",
  "$valuetype",
  "type"
];

gHTMLAttr.param_valuetype =
[
  "data",
  "ref",
  "object"
];


gHTMLAttr.pre =
[
  "%width",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.q =
[
  "cite",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.s =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.samp =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.script =
[
  "charset",
  "$type",
  "language",
  "src",
  "defer"
];

gHTMLAttr.script_defer =
[
  "defer"
];


gHTMLAttr.select =
[
  "name",
  "#size",
  "multiple",
  "disabled",
  "#tabindex",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.select_multiple =
[
  "multiple"
];

gHTMLAttr.select_disabled =
[
  "disabled"
];

gHTMLAttr.small =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.span =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.strike =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.strong =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.style =
[
  "$type",
  "media",
  "title",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.sub =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.sup =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.table =
[
  "summary",
  "%width",
  "#border",
  "frame",
  "rules",
  "#cellspacing",
  "#cellpadding",
  "align",
  "bgcolor",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.table_frame =
[
  "void",
  "above",
  "below",
  "hsides",
  "lhs",
  "rhs",
  "vsides",
  "box",
  "border"
];

gHTMLAttr.table_rules =
[
  "none",
  "groups",
  "rows",
  "cols",
  "all"
];

// Note; This is alignment of the table,
//  not table contents, like all other table child elements
gHTMLAttr.table_align = gHAlign;

gHTMLAttr.table_bgcolor = gHTMLColors;

gHTMLAttr.tbody =
[
  "align",
  "!char",
  "#charoff",
  "valign",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.tbody_align = gHAlignTableContent;

gHTMLAttr.tbody_valign = gVAlignTable;

gHTMLAttr.td =
[
  "abbr",
  "axis",
  "headers",
  "scope",
  "$#rowspan",
  "$#colspan",
  "align",
  "!char",
  "#charoff",
  "valign",
  "nowrap",
  "bgcolor",
  "%width",
  "%height",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.td_scope =
[
  "row",
  "col",
  "rowgroup",
  "colgroup"
];

gHTMLAttr.td_rowspan =
[
  "1" // default
];

gHTMLAttr.td_colspan =
[
  "1" // default
];

gHTMLAttr.td_align = gHAlignTableContent;

gHTMLAttr.td_valign = gVAlignTable;

gHTMLAttr.td_nowrap =
[
  "nowrap"
];

gHTMLAttr.td_bgcolor = gHTMLColors;

gHTMLAttr.textarea =
[
  "name",
  "$#rows",
  "$#cols",
  "disabled",
  "readonly",
  "#tabindex",
  "!accesskey",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.textarea_disabled =
[
  "disabled"
];

gHTMLAttr.textarea_readonly =
[
  "readonly"
];


gHTMLAttr.tfoot =
[
  "align",
  "!char",
  "#charoff",
  "valign",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.tfoot_align = gHAlignTableContent;

gHTMLAttr.tfoot_valign = gVAlignTable;

gHTMLAttr.th =
[
  "abbr",
  "axis",
  "headers",
  "scope",
  "$#rowspan",
  "$#colspan",
  "align",
  "!char",
  "#charoff",
  "valign",
  "nowrap",
  "bgcolor",
  "%width",
  "%height",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.th_scope =
[
  "row",
  "col",
  "rowgroup",
  "colgroup"
];

gHTMLAttr.th_rowspan =
[
  "1" // default
];

gHTMLAttr.th_colspan =
[
  "1" // default
];

gHTMLAttr.th_align = gHAlignTableContent;

gHTMLAttr.th_valign = gVAlignTable;

gHTMLAttr.th_nowrap =
[
  "nowrap"
];

gHTMLAttr.th_bgcolor = gHTMLColors;

gHTMLAttr.thead =
[
  "align",
  "!char",
  "#charoff",
  "valign",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.thead_align = gHAlignTableContent;

gHTMLAttr.thead_valign = gVAlignTable;

gHTMLAttr.title =
[
  "^lang",
  "dir"
];

gHTMLAttr.tr =
[
  "align",
  "!char",
  "#charoff",
  "valign",
  "bgcolor",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.tr_align = gHAlignTableContent;

gHTMLAttr.tr_valign = gVAlignTable;

gHTMLAttr.tr_bgcolor = gHTMLColors;

gHTMLAttr.tt =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.u =
[
  "_core",
  "-",
  "^lang",
  "dir"
];
gHTMLAttr.ul =
[
  "type",
  "compact",
  "-",
  "_core",
  "-",
  "^lang",
  "dir"
];

gHTMLAttr.ul_type =
[
  "disc",
  "square",
  "circle"
];

gHTMLAttr.ul_compact =
[
  "compact"
];


// Prefix with "_" since this is reserved (it's stripped out)
gHTMLAttr._var =
[
  "_core",
  "-",
  "^lang",
  "dir"
];

// ================ JS Attributes ================ //
// These are element specif even handlers. 
/* Most all elements use gCoreJSEvents, so those 
   are assumed except for those listed here with "noEvents"
*/

gJSAttr.a =
[
  "onfocus",
  "onblur"
];

gJSAttr.area =
[
  "onfocus",
  "onblur"
];

gJSAttr.body =
[
  "onload",
  "onupload"
];

gJSAttr.button =
[
  "onfocus",
  "onblur"
];

gJSAttr.form =
[
  "onsubmit",
  "onreset"
];

gJSAttr.frameset =
[
  "onload",
  "onunload"
];

gJSAttr.input =
[
  "onfocus",
  "onblur",
  "onselect",
  "onchange"
];

gJSAttr.label =
[
  "onfocus",
  "onblur"
];

gJSAttr.select =
[
  "onfocus",
  "onblur",
  "onchange"
];

gJSAttr.textarea =
[
  "onfocus",
  "onblur",
  "onselect",
  "onchange"
];

// Elements that don't have JSEvents:
gJSAttr.font =
[
  "noJSEvents"
];

gJSAttr.applet =
[
  "noJSEvents"
];

gJSAttr.isindex =
[
  "noJSEvents"
];

gJSAttr.iframe =
[
  "noJSEvents"
];

