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

/*
 * !!Note this is for a javascriptable pres shell currently for accessing selection
 *   the selection refers only to that which listens to keybindings which is the "NORMAL"
 *   selection this will be IDLIZED LATER
 */

#ifndef nsISelectionControler_h___
#define nsISelectionControler_h___

#include "nsISupports.h"


#define NS_ISELECTIONCONTROLER_IID_STR "D2D1D179-85A7-11d3-9932-00108301233C"

#define NS_ISELECTIONCONTROLER_IID \
{ 0xd2d1d179, 0x85a7, 0x11d3, \
{ 0x99, 0x32, 0x0, 0x10, 0x83, 0x1, 0x23, 0x3c }}


class nsISelectionControler : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ISELECTIONCONTROLER_IID; return iid; }

  /** CharacterMove will move the selection one character forward/backward in the document.
   *  this will also have the effect of collapsing the selection if the aExtend = PR_FALSE
   *  the "point" of selection that is extended is considered the "focus" point. 
   *  or the last point adjusted by the selection.
   *  @param aForward forward or backward if PR_FALSE
   *  @param aExtend  should it collapse the selection of extend it?
   */
  NS_IMETHOD CharacterMove(PRBool aForward, PRBool aExtend) = 0;

  /** WordMove will move the selection one word forward/backward in the document.
   *  this will also have the effect of collapsing the selection if the aExtend = PR_FALSE
   *  the "point" of selection that is extended is considered the "focus" point. 
   *  or the last point adjusted by the selection.
   *  @param aForward forward or backward if PR_FALSE
   *  @param aExtend  should it collapse the selection of extend it?
   */
  NS_IMETHOD WordMove(PRBool aForward, PRBool aExtend) = 0;

  /** LineMove will move the selection one line forward/backward in the document.
   *  this will also have the effect of collapsing the selection if the aExtend = PR_FALSE
   *  the "point" of selection that is extended is considered the "focus" point. 
   *  or the last point adjusted by the selection.
   *  @param aForward forward or backward if PR_FALSE
   *  @param aExtend  should it collapse the selection of extend it?
   */
  NS_IMETHOD LineMove(PRBool aForward, PRBool aExtend) = 0;

  /** IntraLineMove will move the selection to the front of the line or end of the line
   *  in the document.
   *  this will also have the effect of collapsing the selection if the aExtend = PR_FALSE
   *  the "point" of selection that is extended is considered the "focus" point. 
   *  or the last point adjusted by the selection.
   *  @param aForward forward or backward if PR_FALSE
   *  @param aExtend  should it collapse the selection of extend it?
   */
  NS_IMETHOD IntraLineMove(PRBool aForward, PRBool aExtend) = 0;

  /** PageMove will move the selection one page forward/backward in the document.
   *  this will also have the effect of collapsing the selection if the aExtend = PR_FALSE
   *  the "point" of selection that is extended is considered the "focus" point. 
   *  or the last point adjusted by the selection.
   *  @param aForward forward or backward if PR_FALSE
   *  @param aExtend  should it collapse the selection of extend it?
   */
  NS_IMETHOD PageMove(PRBool aForward, PRBool aExtend) = 0;

  /** ScrollPage will scroll the page without affecting the selection.
   *  @param aForward scroll forward or backwards in selection
   */
  NS_IMETHOD ScrollPage(PRBool aForward) = 0;

  /** SelectAll will select the whole page
   */
  NS_IMETHOD SelectAll() = 0;
};



#endif /* nsISelectionControler_h___ */
