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
 * The Original Code is editor atom table.
 *
 * The Initial Developer of the Original Code is
 * Netscape communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
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

/******

  This file contains the list of all editor nsIAtoms and their values
  
  It is designed to be used as inline input to nsEditProperty.cpp *only*
  through the magic of C preprocessing.

  All entries must be enclosed in the macro EDITOR_ATOM which will have cruel
  and unusual things done to it

  It is recommended (but not strictly necessary) to keep all entries
  in alphabetical order

  The first argument to EDITOR_ATOM is the C++ identifier of the atom
  The second argument is the string value of the atom

 ******/


EDITOR_ATOM(b, "b")       
EDITOR_ATOM(big, "big")   
EDITOR_ATOM(i, "i")     
EDITOR_ATOM(small, "small") 
EDITOR_ATOM(strike, "strike")
EDITOR_ATOM(s, "s")
EDITOR_ATOM(sub, "sub")   
EDITOR_ATOM(sup, "sup")   
EDITOR_ATOM(tt, "tt")    
EDITOR_ATOM(u, "u")     
EDITOR_ATOM(em, "em")    
EDITOR_ATOM(strong, "strong")
EDITOR_ATOM(dfn, "dfn")   
EDITOR_ATOM(blink, "blink")  
EDITOR_ATOM(code, "code")  
EDITOR_ATOM(samp, "samp")  
EDITOR_ATOM(kbd, "kbd")   
EDITOR_ATOM(var, "var")   
EDITOR_ATOM(cite, "cite")  
EDITOR_ATOM(abbr, "abbr")  
EDITOR_ATOM(acronym, "acronym")
EDITOR_ATOM(font, "font")  
EDITOR_ATOM(a, "a")     
EDITOR_ATOM(href, "href")     // Use to differentiate between "a" for link, "a" for named anchor
EDITOR_ATOM(name, "name")   
EDITOR_ATOM(img, "img")   
EDITOR_ATOM(object, "object")
EDITOR_ATOM(br, "br")    
EDITOR_ATOM(script, "script")
EDITOR_ATOM(map, "map")   
EDITOR_ATOM(q, "q")     
EDITOR_ATOM(span, "span")  
EDITOR_ATOM(bdo, "bdo")   
EDITOR_ATOM(input, "input") 
EDITOR_ATOM(select, "select")
EDITOR_ATOM(textarea, "textarea")
EDITOR_ATOM(label, "label")
EDITOR_ATOM(button, "button")
  // block tags
EDITOR_ATOM(p, "p")
EDITOR_ATOM(div, "div")
EDITOR_ATOM(center, "center")
EDITOR_ATOM(blockquote, "blockquote")
EDITOR_ATOM(h1, "h1")
EDITOR_ATOM(h2, "h2")
EDITOR_ATOM(h3, "h3")
EDITOR_ATOM(h4, "h4")
EDITOR_ATOM(h5, "h5")
EDITOR_ATOM(h6, "h6")
EDITOR_ATOM(ul, "ul")
EDITOR_ATOM(ol, "ol")
EDITOR_ATOM(dl, "dl")
EDITOR_ATOM(pre, "pre")
EDITOR_ATOM(noscript, "noscript")
EDITOR_ATOM(form, "form")
EDITOR_ATOM(hr, "hr")
EDITOR_ATOM(table, "table")
EDITOR_ATOM(fieldset, "fieldset")
EDITOR_ATOM(address, "address")
  // Unclear from 
  // DTD, block?
EDITOR_ATOM(body, "body")
EDITOR_ATOM(head, "head")
EDITOR_ATOM(html, "html")
EDITOR_ATOM(tr, "tr")
EDITOR_ATOM(td, "td")
EDITOR_ATOM(th, "th")
EDITOR_ATOM(caption, "caption")
EDITOR_ATOM(col, "col")
EDITOR_ATOM(colgroup, "colgroup")
EDITOR_ATOM(tbody, "tbody")
EDITOR_ATOM(thead, "thead")
EDITOR_ATOM(tfoot, "tfoot")
EDITOR_ATOM(li, "li")
EDITOR_ATOM(dt, "dt")
EDITOR_ATOM(dd, "dd")
EDITOR_ATOM(legend, "legend")
  // inline properties
EDITOR_ATOM(color, "color")
EDITOR_ATOM(face, "face")
EDITOR_ATOM(size, "size")
  
EDITOR_ATOM(cssBackgroundColor, "background-color")
EDITOR_ATOM(cssBackgroundImage, "background-image")
EDITOR_ATOM(cssBorder, "border")
EDITOR_ATOM(cssBottom, "bottom")
EDITOR_ATOM(cssCaptionSide, "caption-side")
EDITOR_ATOM(cssColor, "color")
EDITOR_ATOM(cssFloat, "float")
EDITOR_ATOM(cssFontFamily, "font-family")
EDITOR_ATOM(cssFontSize, "font-size")
EDITOR_ATOM(cssFontStyle, "font-style")
EDITOR_ATOM(cssFontWeight, "font-weight")
EDITOR_ATOM(cssHeight, "height")
EDITOR_ATOM(cssListStyleType, "list-style-type")
EDITOR_ATOM(cssLeft, "left")
EDITOR_ATOM(cssMarginRight, "margin-right")
EDITOR_ATOM(cssMarginLeft, "margin-left")
EDITOR_ATOM(cssPosition, "position")
EDITOR_ATOM(cssRight, "right")
EDITOR_ATOM(cssTextAlign, "text-align")
EDITOR_ATOM(cssTextDecoration, "text-decoration")
EDITOR_ATOM(cssTop, "top")
EDITOR_ATOM(cssVerticalAlign, "vertical-align")
EDITOR_ATOM(cssWhitespace, "white-space")
EDITOR_ATOM(cssWidth, "width")
EDITOR_ATOM(cssZIndex, "z-index")

EDITOR_ATOM(cssMozUserSelect, "-moz-user-select")

EDITOR_ATOM(cssPxUnit, "px")
EDITOR_ATOM(cssEmUnit, "em")
EDITOR_ATOM(cssCmUnit, "cm")
EDITOR_ATOM(cssPercentUnit, "%")
EDITOR_ATOM(cssInUnit, "in")
EDITOR_ATOM(cssMmUnit, "mm")
EDITOR_ATOM(cssPtUnit, "pt")
EDITOR_ATOM(cssPcUnit, "pc")
EDITOR_ATOM(cssExUnit, "ex")
