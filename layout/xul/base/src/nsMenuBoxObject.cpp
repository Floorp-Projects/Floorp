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
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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
#include "nsISupportsUtils.h"
#include "nsIMenuBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIMenuFrame.h"
#include "nsIFrame.h"
#include "nsGUIEvent.h"

class nsMenuBoxObject : public nsIMenuBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMENUBOXOBJECT

  nsMenuBoxObject();
  virtual ~nsMenuBoxObject();
  
protected:
};

/* Implementation file */
NS_IMPL_ADDREF(nsMenuBoxObject)
NS_IMPL_RELEASE(nsMenuBoxObject)

NS_IMETHODIMP 
nsMenuBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIMenuBoxObject))) {
    *aResult = (nsIMenuBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsMenuBoxObject::nsMenuBoxObject()
{
}

nsMenuBoxObject::~nsMenuBoxObject()
{
  /* destructor code */
}

/* void openMenu (in boolean openFlag); */
NS_IMETHODIMP nsMenuBoxObject::OpenMenu(PRBool aOpenFlag)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsIMenuFrame* menuFrame;
  CallQueryInterface(frame, &menuFrame);
  if (!menuFrame)
    return NS_OK;

  return menuFrame->OpenMenu(aOpenFlag);
}

NS_IMETHODIMP nsMenuBoxObject::GetActiveChild(nsIDOMElement** aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsIMenuFrame* menuFrame;
  CallQueryInterface(frame, &menuFrame);
  if (!menuFrame)
    return NS_OK;

  return menuFrame->GetActiveChild(aResult);
}

NS_IMETHODIMP nsMenuBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsIMenuFrame* menuFrame;
  CallQueryInterface(frame, &menuFrame);
  if (!menuFrame)
    return NS_OK;

  return menuFrame->SetActiveChild(aResult);
}

/* boolean handleKeyPress (in nsIDOMKeyEvent keyEvent); */
NS_IMETHODIMP nsMenuBoxObject::HandleKeyPress(nsIDOMKeyEvent* aKeyEvent, PRBool* aHandledFlag)
{
  *aHandledFlag = PR_FALSE;
  NS_ENSURE_ARG(aKeyEvent);

  nsIFrame* frame = GetFrame();
  if (!frame)
    return NS_OK;

  nsIMenuFrame* menuFrame;
  CallQueryInterface(frame, &menuFrame);
  if (!menuFrame)
    return NS_OK;

  PRUint32 keyCode;
  aKeyEvent->GetKeyCode(&keyCode);
  switch (keyCode) {
    case NS_VK_UP:
    case NS_VK_DOWN:
    case NS_VK_HOME:
    case NS_VK_END:
      PRBool altKey;
      aKeyEvent->GetAltKey(&altKey);
      if (altKey)
        return NS_OK;
      return menuFrame->KeyboardNavigation(keyCode, *aHandledFlag);
    default:
      return menuFrame->ShortcutNavigation(aKeyEvent, *aHandledFlag);
  }
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewMenuBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsMenuBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

