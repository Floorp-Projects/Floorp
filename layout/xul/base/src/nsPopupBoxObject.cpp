/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCOMPtr.h"
#include "nsIPopupBoxObject.h"
#include "nsIRootBox.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsFrameManager.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsClientRect.h"

class nsPopupBoxObject : public nsBoxObject,
                         public nsIPopupBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPOPUPBOXOBJECT

  nsPopupBoxObject() {}
protected:
  virtual ~nsPopupBoxObject() {}

  nsPopupSetFrame* GetPopupSetFrame();
};

NS_IMPL_ISUPPORTS_INHERITED1(nsPopupBoxObject, nsBoxObject, nsIPopupBoxObject)

nsPopupSetFrame*
nsPopupBoxObject::GetPopupSetFrame()
{
  nsIRootBox* rootBox = nsIRootBox::GetRootBox(GetPresShell(false));
  if (!rootBox)
    return nullptr;

  return rootBox->GetPopupSetFrame();
}

NS_IMETHODIMP
nsPopupBoxObject::HidePopup()
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent)
    pm->HidePopup(mContent, false, true, false);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::ShowPopup(nsIDOMElement* aAnchorElement,
                            nsIDOMElement* aPopupElement,
                            PRInt32 aXPos, PRInt32 aYPos,
                            const PRUnichar *aPopupType,
                            const PRUnichar *aAnchorAlignment,
                            const PRUnichar *aPopupAlignment)
{
  NS_ENSURE_TRUE(aPopupElement, NS_ERROR_INVALID_ARG);
  // srcContent can be null.

  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));
    nsAutoString popupType(aPopupType);
    nsAutoString anchor(aAnchorAlignment);
    nsAutoString align(aPopupAlignment);
    pm->ShowPopupWithAnchorAlign(mContent, anchorContent, anchor, align,
                                 aXPos, aYPos, popupType.EqualsLiteral("context"));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::OpenPopup(nsIDOMElement* aAnchorElement,
                            const nsAString& aPosition,
                            PRInt32 aXPos, PRInt32 aYPos,
                            bool aIsContextMenu,
                            bool aAttributesOverride,
                            nsIDOMEvent* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));
    pm->ShowPopup(mContent, anchorContent, aPosition, aXPos, aYPos,
                  aIsContextMenu, aAttributesOverride, false, aTriggerEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::OpenPopupAtScreen(PRInt32 aXPos, PRInt32 aYPos,
                                    bool aIsContextMenu,
                                    nsIDOMEvent* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent)
    pm->ShowPopupAtScreen(mContent, aXPos, aYPos, aIsContextMenu, aTriggerEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::MoveTo(PRInt32 aLeft, PRInt32 aTop)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    menuPopupFrame->MoveTo(aLeft, aTop, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SizeTo(PRInt32 aWidth, PRInt32 aHeight)
{
  if (!mContent)
    return NS_OK;

  nsAutoString width, height;
  width.AppendInt(aWidth);
  height.AppendInt(aHeight);

  nsCOMPtr<nsIContent> content = mContent;
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::width, width, false);
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::height, height, true);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAutoPosition(bool* aShouldAutoPosition)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    *aShouldAutoPosition = menuPopupFrame->GetAutoPosition();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SetAutoPosition(bool aShouldAutoPosition)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    menuPopupFrame->SetAutoPosition(aShouldAutoPosition);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::EnableRollup(bool aShouldRollup)
{
  // this does nothing now
  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SetConsumeRollupEvent(PRUint32 aConsume)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    menuPopupFrame->SetConsumeRollupEvent(aConsume);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::EnableKeyboardNavigator(bool aEnableKeyboardNavigator)
{
  if (!mContent)
    return NS_OK;

  // Use ignorekeys="true" on the popup instead of using this function.
  if (aEnableKeyboardNavigator)
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::ignorekeys, true);
  else
    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::ignorekeys,
                      NS_LITERAL_STRING("true"), true);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetPopupState(nsAString& aState)
{
  // set this here in case there's no frame for the popup
  aState.AssignLiteral("closed");

  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    switch (menuPopupFrame->PopupState()) {
      case ePopupShowing:
      case ePopupOpen:
        aState.AssignLiteral("showing");
        break;
      case ePopupOpenAndVisible:
        aState.AssignLiteral("open");
        break;
      case ePopupHiding:
      case ePopupInvisible:
        aState.AssignLiteral("hiding");
        break;
      case ePopupClosed:
        break;
      default:
        NS_NOTREACHED("Bad popup state");
        break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetTriggerNode(nsIDOMNode** aTriggerNode)
{
  *aTriggerNode = nullptr;

  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  nsIContent* triggerContent = nsMenuPopupFrame::GetTriggerContent(menuPopupFrame);
  if (triggerContent)
    CallQueryInterface(triggerContent, aTriggerNode);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAnchorNode(nsIDOMElement** aAnchor)
{
  *aAnchor = nullptr;

  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame)
    return NS_OK;

  nsIContent* anchor = menuPopupFrame->GetAnchor();
  if (anchor)
    CallQueryInterface(anchor, aAnchor);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetOuterScreenRect(nsIDOMClientRect** aRect)
{
  nsClientRect* rect = new nsClientRect();
  if (!rect)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aRect = rect);

  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame)
    return NS_OK;

  // Return an empty rectangle if the popup is not open.
  nsPopupState state = menuPopupFrame->PopupState();
  if (state != ePopupOpen && state != ePopupOpenAndVisible)
    return NS_OK;

  nsIView* view = menuPopupFrame->GetView();
  if (view) {
    nsIWidget* widget = view->GetWidget();
    if (widget) {
      nsIntRect screenRect;
      widget->GetScreenBounds(screenRect);

      PRInt32 pp = menuPopupFrame->PresContext()->AppUnitsPerDevPixel();
      rect->SetLayoutRect(screenRect.ToAppUnits(pp));
    }
  }

  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPopupBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsPopupBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
