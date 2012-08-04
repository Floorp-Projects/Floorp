/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsISupportsUtils.h"
#include "nsIMenuBoxObject.h"
#include "nsBoxObject.h"
#include "nsIFrame.h"
#include "nsGUIEvent.h"
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
        nsMenuFrame* menu = do_QueryFrame(frame);
        if (menu) {
          nsMenuPopupFrame* popupFrame = menu->GetPopup();
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
  *aResult = nullptr;
  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (menu)
    return menu->GetActiveChild(aResult);
  return NS_OK;
}

NS_IMETHODIMP nsMenuBoxObject::SetActiveChild(nsIDOMElement* aResult)
{
  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (menu)
    return menu->SetActiveChild(aResult);
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
  bool eventHandled = false;
  aKeyEvent->GetPreventDefault(&eventHandled);
  if (eventHandled)
    return NS_OK;

  if (nsMenuBarListener::IsAccessKeyPressed(aKeyEvent))
    return NS_OK;

  nsMenuFrame* menu = do_QueryFrame(GetFrame(false));
  if (!menu)
    return NS_OK;

  nsMenuPopupFrame* popupFrame = menu->GetPopup();
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

  nsMenuFrame* menuframe = do_QueryFrame(GetFrame(false));
  if (!menuframe)
    return NS_OK;

  nsIFrame* frame = menuframe->GetParent();
  while (frame) {
    nsMenuBarFrame* menubar = do_QueryFrame(frame);
    if (menubar) {
      *aOpenedWithKey = menubar->IsActiveByKeyboard();
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

