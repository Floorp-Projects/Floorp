/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

var s;
for (s = 0; s < document.styleSheets.length; s++) {
   var sheet = document.styleSheets[s];
   dump("Style sheet #" + (s+1) + ": " + sheet.title + "\n");
   var i, r;
   dump("\n");
   for (i = 0; i < sheet.imports.length; i++) {
      dump("@import url(" + sheet.imports[i].href + ");\n");
   }
   dump("\n");
   for (r = 0; r < sheet.rules.length; r++) {
     var rule = sheet.rules[r];
     dump(rule.selectorText + "  {" + "\n");
     var style = rule.style;
     var p;
     for (p = 0; p < style.length; p++) {
	dump("    " + style[p] + ":" + style.getPropertyValue(style[p]) + ";\n");
     }
     dump("    }\n");
   }
   dump("\n");
} 