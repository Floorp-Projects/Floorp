/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
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
NS_IMETHODIMP nsMenuBoxObject::OpenMenu(bool aOpenFlag)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsIFrame* frame = GetFrame(false);
    if (frame) {
      if (aOpenFlag) {
        nsCOMPtr<nsIContent> content = mContent;
        pm->ShowMenu(content, false, false);
      }
      else {
        if (frame->GetType() == nsGkAtoms::menuFrame) {
          nsMenuPopupFrame* popupFrame = (static_cast<nsMenuFrame *>(frame))->GetPopup();
          if (popupFrame)
            pm->HidePopup(popupFrame->GetContent(), false, true, false);
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsMenuBoxObject::GetActiveChild(nsIDOMElement** aResult)
{
  *aResult = nsnull;
  nsIFrame* frame = GetFrame(false);
  if (frame && frame->GetType() == nsGkAtoms::menuFrame)
    return static_cast<nsMenuFrame *>(frame)->GetActiveChild(aResult);
  return NS_OK;
}

NS_IMETHODIMP nsMenuBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsIFrame* frame = GetFrame(false);
  if (frame && frame->GetType() == nsGkAtoms::menuFrame)
    return static_cast<nsMenuFrame *>(frame)->SetActiveChild(aResult);
  return NS_OK;
}

/* boolean handleKeyPress (in nsIDOMKeyEvent keyEvent); */
NS_IMETHODIMP nsMenuBoxObject::HandleKeyPress(nsIDOMKeyEvent* aKeyEvent, bool* aHandledFlag)
{
  *aHandledFlag = false;
  NS_ENSURE_ARG(aKeyEvent);

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (!pm)
    return NS_OK;

  // if event has already been handled, bail
  nsCOMPtr<nsIDOMNSEvent> domNSEvent = do_QueryInterface(aKeyEvent);
  if (!domNSEvent)
    return NS_OK;

  bool eventHandled = false;
  domNSEvent->GetPreventDefault(&eventHandled);
  if (eventHandled)
    return NS_OK;

  if (nsMenuBarListener::IsAccessKeyPressed(aKeyEvent))
    return NS_OK;

  nsIFrame* frame = GetFrame(false);
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
nsMenuBoxObject::GetOpenedWithKey(bool* aOpenedWithKey)
{
  *aOpenedWithKey = false;

  nsIFrame* frame = GetFrame(false);
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

