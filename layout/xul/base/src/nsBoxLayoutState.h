/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/**
 
  Author:
  Eric D Vaughan

**/

#ifndef nsBoxLayoutState_h___
#define nsBoxLayoutState_h___

#include "nsIFrame.h"
#include "nsCOMPtr.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"

class nsRenderingContext;
class nsCalculatedBoxInfo;
struct nsHTMLReflowMetrics;
class nsString;
class nsHTMLReflowCommand;

class NS_STACK_CLASS nsBoxLayoutState
{
public:
  nsBoxLayoutState(nsPresContext* aPresContext,
                   nsRenderingContext* aRenderingContext = nsnull,
                   // see OuterReflowState() below
                   const nsHTMLReflowState* aOuterReflowState = nsnull,
                   PRUint16 aReflowDepth = 0) NS_HIDDEN;
  nsBoxLayoutState(const nsBoxLayoutState& aState) NS_HIDDEN;

  nsPresContext* PresContext() const { return mPresContext; }
  nsIPresShell* PresShell() const { return mPresContext->PresShell(); }

  PRUint32 LayoutFlags() const { return mLayoutFlags; }
  void SetLayoutFlags(PRUint32 aFlags) { mLayoutFlags = aFlags; }

  // if true no one under us will paint during reflow.
  void SetPaintingDisabled(bool aDisable) { mPaintingDisabled = aDisable; }
  bool PaintingDisabled() const { return mPaintingDisabled; }

  // The rendering context may be null for specialized uses of
  // nsBoxLayoutState and should be null-checked before it is used.
  // However, passing a null rendering context to the constructor when
  // doing box layout or intrinsic size calculation will cause bugs.
  nsRenderingContext* GetRenderingContext() const { return mRenderingContext; }

  struct AutoReflowDepth {
    AutoReflowDepth(nsBoxLayoutState& aState)
      : mState(aState) { ++mState.mReflowDepth; }
    ~AutoReflowDepth() { --mState.mReflowDepth; }
    nsBoxLayoutState& mState;
  };

  // The HTML reflow state that lives outside the box-block boundary.
  // May not be set reliably yet.
  const nsHTMLReflowState* OuterReflowState() { return mOuterReflowState; }

  PRUint16 GetReflowDepth() { return mReflowDepth; }
  
private:
  nsRefPtr<nsPresContext> mPresContext;
  nsRenderingContext *mRenderingContext;
  const nsHTMLReflowState *mOuterReflowState;
  PRUint32 mLayoutFlags;
  PRUint16 mReflowDepth; 
  bool mPaintingDisabled;
};

#endif

