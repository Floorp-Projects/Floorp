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

  eStyleStruct_Max            = eStyleStruct_Outline,

  eStyleStruct_BorderPaddingShortcut = 15       // only for use in GetStyle()
};


struct nsStyleStruct {
};


#endif /* nsStyleStruct_h___ */

