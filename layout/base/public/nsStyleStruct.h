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

#ifndef nsStyleStruct_h___
#define nsStyleStruct_h___


// If you add structures in that list, look for the comments tagged with
// the string //#insert new style structs here# in "nsStyleContext.cpp".
// You should also add a helper class in "nsIMutableStyleContext.h"
enum nsStyleStructID {
  eStyleStruct_Font           = 1,
  eStyleStruct_Color          = 2,
  eStyleStruct_List           = 3,
  eStyleStruct_Position       = 4,
  eStyleStruct_Text           = 5,
  eStyleStruct_Display        = 6,
  eStyleStruct_Table          = 7,
  eStyleStruct_Content        = 8,
  eStyleStruct_UserInterface  = 9,
  eStyleStruct_Print					= 10,
  eStyleStruct_Margin         = 11,
  eStyleStruct_Padding        = 12,
  eStyleStruct_Border         = 13,
  eStyleStruct_Outline        = 14,
#ifdef INCLUDE_XUL
  eStyleStruct_XUL            = 15,
  eStyleStruct_Max            = eStyleStruct_XUL,
#else
  eStyleStruct_Max            = eStyleStruct_Outline,
#endif
  eStyleStruct_BorderPaddingShortcut = 99       // only for use in GetStyle()
};

class nsIPresContext;

struct nsStyleStruct {
};


#endif /* nsStyleStruct_h___ */

