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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsBoxObject.h"
#include "nsIBoxLayoutManager.h"
#include "nsIBoxPaintManager.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsIFrame.h"
#include "nsXPIDLString.h"

// Static IIDs/CIDs. Try to minimize these.
// None so far.

// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS3(nsBoxObject, nsIBoxObject, nsPIBoxObject, nsISecurityCheckedComponent)

// Constructors/Destructors
nsBoxObject::nsBoxObject(void)
:mContent(nsnull), mPresShell(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsBoxObject::~nsBoxObject(void)
{
}

NS_IMETHODIMP
nsBoxObject::GetLayoutManager(nsIBoxLayoutManager** aResult)
{
  *aResult = mLayoutManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetLayoutManager(nsIBoxLayoutManager* aLayoutManager)
{
  mLayoutManager = aLayoutManager;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPaintManager(nsIBoxPaintManager** aResult)
{
  *aResult = mPaintManager;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetPaintManager(nsIBoxPaintManager* aPaintManager)
{
  mPaintManager = aPaintManager;
  return NS_OK;
}

// nsPIBoxObject //////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsBoxObject::Init(nsIContent* aContent, nsIPresShell* aShell)
{
  mContent = aContent;
  mPresShell = aShell;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetDocument(nsIDocument* aDocument)
{
  mPresState = nsnull;
  if (aDocument) {
    nsCOMPtr<nsIPresShell> shell = getter_AddRefs(aDocument->GetShellAt(0));
    mPresShell = shell;
  }
  else {
    mPresShell = nsnull;
  }
  return NS_OK;
}

nsIFrame*
nsBoxObject::GetFrame()
{
  nsIFrame* frame = nsnull;
  if (mPresShell)
    mPresShell->GetPrimaryFrameFor(mContent, &frame);
  return frame;
}

nsresult 
nsBoxObject::GetOffsetRect(nsRect& aRect)
{
  nsresult res = NS_OK;

  aRect.x = aRect.y = 0;
  aRect.Empty();
 
  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));

  if (doc) {
    // Get Presentation shell 0
    nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(doc->GetShellAt(0));

    if(presShell) {
      // Flush all pending notifications so that our frames are uptodate
      presShell->FlushPendingNotifications();

      // Get the Frame for our content
      nsIFrame* frame = nsnull;
      presShell->GetPrimaryFrameFor(mContent, &frame);
      if(frame != nsnull) {
        // Get its origin
        nsPoint origin;
        frame->GetOrigin(origin);

        // Get the union of all rectangles in this and continuation frames
        nsRect rcFrame;
        nsIFrame* next = frame;
        do {
          nsRect rect;
          next->GetRect(rect);
          rcFrame.UnionRect(rcFrame, rect);
          next->GetNextInFlow(&next);
        } while (nsnull != next);
        

        // Find the frame parent whose content's tagName either matches 
        // the tagName passed in or is the document element.
        nsCOMPtr<nsIContent> docElement = getter_AddRefs(doc->GetRootContent());
        nsIFrame* parent = frame;
        nsCOMPtr<nsIContent> parentContent;
        frame->GetParent(&parent);
        while (parent) {
          parent->GetContent(getter_AddRefs(parentContent));
          if (parentContent) {
            // If we've hit the document element, break here
            if (parentContent.get() == docElement.get()) {
              break;
            }
          }

          // Add the parent's origin to our own to get to the
          // right coordinate system
          nsPoint parentOrigin;
          parent->GetOrigin(parentOrigin);
          origin += parentOrigin;

          parent->GetParent(&parent);
        }
  
        // For the origin, add in the border for the frame
        const nsStyleSpacing* spacing;
        nsStyleCoord coord;
        frame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
        if (spacing) {
          if (eStyleUnit_Coord == spacing->mBorder.GetLeftUnit()) {
            origin.x += spacing->mBorder.GetLeft(coord).GetCoordValue();
          }
          if (eStyleUnit_Coord == spacing->mBorder.GetTopUnit()) {
            origin.y += spacing->mBorder.GetTop(coord).GetCoordValue();
          }
        }

        // And subtract out the border for the parent
        if (parent) {
          const nsStyleSpacing* parentSpacing;
          parent->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)parentSpacing);
          if (parentSpacing) {
            if (eStyleUnit_Coord == parentSpacing->mBorder.GetLeftUnit()) {
              origin.x -= parentSpacing->mBorder.GetLeft(coord).GetCoordValue();
            }
            if (eStyleUnit_Coord == parentSpacing->mBorder.GetTopUnit()) {
              origin.y -= parentSpacing->mBorder.GetTop(coord).GetCoordValue();
            }
          }
        }

        // Get the Presentation Context from the Shell
        nsCOMPtr<nsIPresContext> context;
        presShell->GetPresContext(getter_AddRefs(context));
       
        if(context) {
          // Get the scale from that Presentation Context
          float scale;
          context->GetTwipsToPixels(&scale);
              
          // Convert to pixels using that scale
          aRect.x = NSTwipsToIntPixels(origin.x, scale);
          aRect.y = NSTwipsToIntPixels(origin.y, scale);
          aRect.width = NSTwipsToIntPixels(rcFrame.width, scale);
          aRect.height = NSTwipsToIntPixels(rcFrame.height, scale);
        }
      }
    }
  }
 
  return res;
}  

NS_IMETHODIMP
nsBoxObject::GetX(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.x;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetY(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.y;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetWidth(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.width;
  return NS_OK;
}

NS_IMETHODIMP 
nsBoxObject::GetHeight(PRInt32* aResult)
{
  nsRect rect;
  GetOffsetRect(rect);
  *aResult = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::GetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports** aResult)
{
  if (!mPresState) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsAutoString propertyName(aPropertyName);
  return mPresState->GetStatePropertyAsSupports(propertyName, aResult); // Addref here.
}

NS_IMETHODIMP
nsBoxObject::SetPropertyAsSupports(const PRUnichar* aPropertyName, nsISupports* aValue)
{
  if (!mPresState)
    NS_NewPresState(getter_AddRefs(mPresState));

  nsAutoString propertyName(aPropertyName);
  return mPresState->SetStatePropertyAsSupports(propertyName, aValue);
}

NS_IMETHODIMP
nsBoxObject::GetProperty(const PRUnichar* aPropertyName, PRUnichar** aResult)
{
  if (!mPresState) {
    *aResult = nsnull;
    return NS_OK;
  }

  nsAutoString propertyName(aPropertyName);
  nsAutoString result;
  nsresult rv = mPresState->GetStateProperty(propertyName, result);
  if (NS_FAILED(rv))
    return rv;
  *aResult = nsXPIDLString::Copy(result.GetUnicode());
  return NS_OK;
}

NS_IMETHODIMP
nsBoxObject::SetProperty(const PRUnichar* aPropertyName, const PRUnichar* aPropertyValue)
{
  if (!mPresState)
    NS_NewPresState(getter_AddRefs(mPresState));

  nsAutoString propertyName(aPropertyName);
  nsAutoString propertyValue(aPropertyValue);
  return mPresState->SetStateProperty(propertyName, propertyValue);
}

NS_IMETHODIMP
nsBoxObject::RemoveProperty(const PRUnichar* aPropertyName)
{
  if (!mPresState)
    return NS_OK;

  nsAutoString propertyName(aPropertyName);
  return mPresState->RemoveStateProperty(propertyName);
}

/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP nsBoxObject::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP nsBoxObject::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP nsBoxObject::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP nsBoxObject::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}
// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
