/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsICaret_h__
#define nsICaret_h__

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"

class nsIRenderingContext;
class nsIFrame;
class nsIView;
class nsIPresShell;
struct nsRect;
struct nsPoint;
class nsISelection;

// IID for the nsICaret interface
#define NS_ICARET_IID       \
{ 0xa6cf90e1, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nsICaret: public nsISupports
{
public:
  static const nsIID& GetIID() { static nsIID iid = NS_ICARET_IID; return iid; }

  typedef enum EViewCoordinates {
      eTopLevelWindowCoordinates,
      eRenderingViewCoordinates,
      eClosestViewCoordinates,
      eIMECoordinates
    } EViewCoordinates;

  NS_IMETHOD Init(nsIPresShell *inPresShell) = 0;
  
  NS_IMETHOD SetCaretDOMSelection(nsISelection *aDOMSel) = 0;

  /** SetCaretVisible will set the visibility of the caret
   *  @param inMakeVisible PR_TRUE to show the caret, PR_FALSE to hide it
   */
  NS_IMETHOD SetCaretVisible(PRBool inMakeVisible) = 0;

  /** GetCaretVisible will get the visibility of the caret
   *  @param inMakeVisible PR_TRUE it is shown, PR_FALSE it is hidden
   */
  NS_IMETHOD GetCaretVisible(PRBool *outMakeVisible) = 0;

  /** SetCaretReadOnly set the appearance of the caret
   *  @param inMakeReadonly PR_TRUE to show the caret in a 'read only' state,
   *  PR_FALSE to show the caret in normal, editing state
   */
  NS_IMETHOD SetCaretReadOnly(PRBool inMakeReadonly) = 0;

  /** GetCaretCoordinates
   *  Get the position of the caret in coordinates relative to the typed specified (aRelativeToType).
   *  If the selection is collapsed, this returns the caret location
   *    and true in outIsCollapsed.
   *  If the selection is not collapsed, this returns the location of the focus pos,
   *    and false in outIsCollapsed.
   */
  NS_IMETHOD GetCaretCoordinates(EViewCoordinates aRelativeToType, nsISelection *aDOMSel, nsRect *outCoordinates, PRBool *outIsCollapsed) = 0;

  /** ClearFrameRefs
   *  The caret stores a reference to the frame that the caret was last drawn in.
   *  This is called to tell the caret that that frame is going away.
   */
  NS_IMETHOD ClearFrameRefs(nsIFrame* aFrame) = 0;
  /** Erase Caret
   *  this will erase the caret if its drawn and reset drawn status
   */
  NS_IMETHOD EraseCaret() = 0;

  /** Set Caret Width
   *  this will set the caret width to the passed in value.
   */
  NS_IMETHOD SetCaretWidth(nscoord aPixels) = 0;

};

extern nsresult NS_NewCaret(nsICaret** aInstancePtrResult);


// handy stack-based class for temporarily disabling the caret

class StCaretHider
{
public:
               StCaretHider(nsICaret* aSelCon)
               : mWasVisible(PR_FALSE), mCaret(aSelCon)
               {
                 if (mCaret)
                 {
                   mCaret->GetCaretVisible(&mWasVisible);
                   if (mWasVisible)
                     mCaret->SetCaretVisible(PR_FALSE);
                 }
               }
               
               ~StCaretHider()
               {
                 if (mCaret && mWasVisible)
                   mCaret->SetCaretVisible(PR_TRUE);
                 // nsCOMPtr releases mPresShell
               }

protected:

    PRBool                  mWasVisible;
    nsCOMPtr<nsICaret>  mCaret;
};


#endif  // nsICaret_h__

