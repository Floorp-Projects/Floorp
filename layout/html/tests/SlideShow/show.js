/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

var body = document.documentElement.childNodes[1];
var center = body.childNodes[1];
var image = center.childNodes[1];

var images = new Array(10);
images[0] = "0.JPG";
images[1] = "1.JPG";
images[2] = "2.JPG";
images[3] = "3.JPG";
images[4] = "4.JPG";
images[5] = "5.JPG";
images[6] = "6.JPG";
images[7] = "7.JPG";
images[8] = "8.JPG";
images[9] = "9.JPG";

var index = 0;

function slideShow()
{
  image.setAttribute("SRC", images[index]);
  index++;
  if (index >= 10) {
    index = 0;
  }
}

var int1 = setInterval(slideShow, 500);
