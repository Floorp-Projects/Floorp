/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

var body = document.documentElement.childNodes[1];
var para = body.childNodes[1];
var img1 = para.childNodes[5];
var tbody = document.getElementsByTagName("TBODY")[0];
var tparent = tbody.childNodes[0].childNodes[0];
var img2 = tparent.childNodes[0].childNodes[0];

var pos = 0;

function rotate(p, c) {
    p.removeChild(c);
    var child = p.childNodes[pos++];
    if (pos > p.childNodes.length) {
      pos = 0;
    }  
    p.insertBefore(c, child);
}

function rotate2() {
  tparent.childNodes[0].removeChild(img2);
  if (tparent.nextSibling != null) {
    tparent = tparent.nextSibling;
  }
  else if (tparent.parentNode.nextSibling != null) {
    tparent = tparent.parentNode.nextSibling.childNodes[0];
  }
  else {
    tparent = tbody.childNodes[0].childNodes[0];
  }
  tparent.childNodes[0].insertBefore(img2, tparent.childNodes[0].childNodes[0]);
}

var int1 = setInterval(rotate, 1000, para, img1);
var int2 = setInterval(rotate2, 1000)
