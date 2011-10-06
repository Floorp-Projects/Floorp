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
#include "nsIFrame.h"
#include "nsGUIEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsMenuBarFrame.h"
#include "nsMenuBarListener.h"
#include "nsMenuFrame.h"
#include "nsMenuPopupFrame.h"

class nsMenuBoxObject : public nsIMenuBoxObject,
                        public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIMENUBOXOBJECT

  nsMenuBoxObject();
  virtual ~nsMenuBoxObject();
};

nsMenuBoxObject::nsMenuBoxObject()
{
}

nsMenuBoxObject::~nsMenuBoxObject()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsMenuBoxObject, nsBoxObject, nsIMenuBoxObject)

/* void openMenu (in boolean openFlag); */
NS_IMETHODIMP nsMenuBoxObject::OpenMenu(PRBool aOpenFlag)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* frame = GetFrame(PR_FALSE);
    if (frame) {
      if (aOpenFlag) {
        nsCOMPtr<nsIContent> content = mContent;
        pm->ShowMenu(content, PR_FALSE, PR_FALSE);
      }
      else {
        if (frame->GetType() == nsGkAtoms::menuFrame) {
          nsMenuPopupFrame* popupFrame = (static_cast<nsMenuFrame *>(frame))->GetPopup();
          if (popupFrame)
            pm->HidePopup(popupFrame->GetContent(), PR_FALSE, PR_TRUE, PR_FALSE);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMenuBoxObject::GetActiveChild(nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (frame && frame->GetType() == nsGkAtoms::menuFrame)
    return static_cast<nsMenuFrame *>(frame)->GetActiveChild(aResult);
  return NS_OK;
}

NS_IMETHODIMP nsMenuBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsIFrame* frame = GetFrame(PR_FALSE);
  if (frame && frame->GetType() == nsGkAtoms::menuFrame)
    return static_cast<nsMenuFrame *>(frame)->SetActiveChild(aResult);
  return NS_OK;
}

/* boolean handleKeyPress (in nsIDOMKeyEvent keyEvent); */
NS_IMETHODIMP nsMenuBoxObject::HandleKeyPress(nsIDOMKeyEvent* aKeyEvent, PRBool* aHandledFlag)
{
  *aHandledFlag = PR_FALSE;
  NS_ENSURE_ARG(aKeyEvent);

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm)
    return NS_OK;

  // if event has already been handled, bail
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  if (!domNSEvent)
    return NS_OK;

  PRBool eventHandled = PR_FALSE;
  domNSEvent->GetPreventDefault(&eventHandled);
  if (eventHandled)
    return NS_OK;

  if (nsMenuBarListener::IsAccessKeyPressed(aKeyEvent))
    return NS_OK;

  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame || frame->GetType() != nsGkAtoms::menuFrame)
    return NS_OK;

  nsMenuPopupFrame* popupFrame = static_cast<nsMenuFrame *>(frame)->GetPopup();
  if (!popupFrame)
    return NS_OK;

  PRUint32 keyCode;
  aKeyEvent->GetKeyCode(&keyCode);
  switch (keyCode) {
    case NS_VK_UP:
    case NS_VK_DOWN:
    case NS_VK_HOME:
    case NS_VK_END:
    {
      nsNavigationDirection theDirection;
      theDirection = NS_DIRECTION_FROM_KEY_CODE(popupFrame, keyCode);
      *aHandledFlag =
        pm->HandleKeyboardNavigationInPopup(popupFrame, theDirection);
      return NS_OK;
    }
    default:
      *aHandledFlag = pm->HandleShortcutNavigation(aKeyEvent, popupFrame);
      return NS_OK;
  }
}

NS_IMETHODIMP
nsMenuBoxObject::GetOpenedWithKey(PRBool* aOpenedWithKey)
{
  *aOpenedWithKey = PR_FALSE;

  nsIFrame* frame = GetFrame(PR_FALSE);
  if (!frame || frame->GetType() != nsGkAtoms::menuFrame)
    return NS_OK;

  frame = frame->GetParent();
  while (frame) {
    if (frame->GetType() == nsGkAtoms::menuBarFrame) {
      *aOpenedWithKey = (static_cast<nsMenuBarFrame *>(frame))->IsActiveByKeyboard();
      return NS_OK;
    }
    frame = frame->GetParent();
  }

  return NS_OK;
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

