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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsMenuDismissalListener.h"
#include "nsIMenuParent.h"
#include "nsMenuFrame.h"

/*
 * nsMenuDismissalListener implementation
 */

NS_IMPL_ADDREF(nsMenuDismissalListener)
NS_IMPL_RELEASE(nsMenuDismissalListener)
NS_IMPL_QUERY_INTERFACE3(nsMenuDismissalListener, nsIDOMMouseListener, nsIMenuRollup, nsIRollupListener)


////////////////////////////////////////////////////////////////////////

nsMenuDismissalListener::nsMenuDismissalListener() :
  mEnabled(PR_TRUE)
{
  NS_INIT_REFCNT();
  mMenuParent = nsnull;
}

////////////////////////////////////////////////////////////////////////
nsMenuDismissalListener::~nsMenuDismissalListener() 
{
}

////////////////////////////////////////////////////////////////////////
nsresult
nsMenuDismissalListener::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

void
nsMenuDismissalListener::SetCurrentMenuParent(nsIMenuParent* aMenuParent)
{
  if (aMenuParent == mMenuParent)
    return;

  nsCOMPtr<nsIRollupListener> kungFuDeathGrip = this;
  Unregister();
  
  mMenuParent = aMenuParent;
  if (!aMenuParent)
    return;

  nsCOMPtr<nsIWidget> widget;
  aMenuParent->GetWidget(getter_AddRefs(widget));
  if (!widget)
    return;

  widget->CaptureRollupEvents(this, PR_TRUE, PR_TRUE);
  mWidget = widget;

  NS_ADDREF(nsMenuFrame::sDismissalListener = this);
}

NS_IMETHODIMP
nsMenuDismissalListener::Rollup()
{
  if (mEnabled) {
    if (mMenuParent) {
      AddRef();
      mMenuParent->HideChain();
      mMenuParent->DismissChain();
      Release();
    }
    else
      Unregister();
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsMenuDismissalListener::ShouldRollupOnMouseWheelEvent(PRBool *aShouldRollup) 
{ 
  *aShouldRollup = PR_FALSE; 
  return NS_OK;
}


// uggggh.


NS_IMETHODIMP
nsMenuDismissalListener::GetSubmenuWidgetChain(nsISupportsArray **_retval)
{
  NS_NewISupportsArray ( _retval );
  nsCOMPtr<nsIMenuParent> curr ( dont_QueryInterface(mMenuParent) );
  while ( curr ) {
    nsCOMPtr<nsIWidget> widget;
    curr->GetWidget ( getter_AddRefs(widget) );
    nsCOMPtr<nsISupports> genericWidget ( do_QueryInterface(widget) );
    (**_retval).AppendElement ( genericWidget );
    
    // move up the chain
    nsIFrame* currAsFrame = nsnull;
    if ( NS_SUCCEEDED(curr->QueryInterface(NS_GET_IID(nsIFrame), NS_REINTERPRET_CAST(void**,&currAsFrame))) ) {
      nsIFrame* parentFrame = nsnull;
      currAsFrame->GetParent(&parentFrame);
      nsIMenuParent* next;
      nsCOMPtr<nsIMenuFrame> menuFrame ( do_QueryInterface(parentFrame) );
      if ( menuFrame ) {
        menuFrame->GetMenuParent ( &next );       // Advance to next parent
        curr = dont_AddRef(next);
      }
      else {
        // we are a menuParent but not a menuFrame. This is probably the case
        // of the menu bar. Nothing to do here, really.
        return NS_OK;
      }
    }
    else {
      // We've run into a menu parent that isn't a frame at all. Not good.
      NS_WARNING ( "nsIMenuParent that is not a nsIFrame" );
      return NS_ERROR_FAILURE;
    }
  } // foreach parent menu
  
  return NS_OK; 
}

#if 0
NS_IMETHODIMP
nsMenuDismissalListener::FirstMenuParent(nsIMenuParent * *_retval)
{
  NS_IF_ADDREF(*_retval = mMenuParent);
  
  return NS_OK;
}


NS_IMETHODIMP
nsMenuDismissalListener::NextMenuParent(nsIMenuParent * inCurrent, nsIMenuParent * *_retval)
{
//XXX for now don't return anything in the chain
  *_retval = nsnull;
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsMenuDismissalListener::Unregister()
{
  if (mWidget) {
    mWidget->CaptureRollupEvents(this, PR_FALSE, PR_FALSE);
    mWidget = nsnull;
  }
  
  NS_RELEASE(nsMenuFrame::sDismissalListener);
  return NS_OK;
}

NS_IMETHODIMP
nsMenuDismissalListener::EnableListener(PRBool aEnabled)
{
  mEnabled = aEnabled;
  return NS_OK;
}

