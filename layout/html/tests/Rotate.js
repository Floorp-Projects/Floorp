/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

var body = document.documentElement.childNodes[1];
var para = body.childNodes[1];
var img1 = para.childNodes[5];
var tbody = body.childNodes[2].childNodes[1];
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
