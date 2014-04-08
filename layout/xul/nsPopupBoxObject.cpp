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
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsView.h"
#include "mozilla/AppUnits.h"
#include "mozilla/dom/DOMRect.h"

using namespace mozilla::dom;

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
    pm->HidePopup(mContent, false, true, false, false);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::ShowPopup(nsIDOMElement* aAnchorElement,
                            nsIDOMElement* aPopupElement,
                            int32_t aXPos, int32_t aYPos,
                            const char16_t *aPopupType,
                            const char16_t *aAnchorAlignment,
                            const char16_t *aPopupAlignment)
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
                            int32_t aXPos, int32_t aYPos,
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
nsPopupBoxObject::OpenPopupAtScreen(int32_t aXPos, int32_t aYPos,
                                    bool aIsContextMenu,
                                    nsIDOMEvent* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent)
    pm->ShowPopupAtScreen(mContent, aXPos, aYPos, aIsContextMenu, aTriggerEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::MoveTo(int32_t aLeft, int32_t aTop)
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    menuPopupFrame->MoveTo(aLeft, aTop, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::MoveToAnchor(nsIDOMElement* aAnchorElement,
                               const nsAString& aPosition,
                               int32_t aXPos, int32_t aYPos,
                               bool aAttributesOverride)
{
  if (mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));

    nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(mContent->GetPrimaryFrame());
    if (menuPopupFrame && menuPopupFrame->PopupState() == ePopupOpenAndVisible) {
      menuPopupFrame->MoveToAnchor(anchorContent, aPosition, aXPos, aYPos, aAttributesOverride);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SizeTo(int32_t aWidth, int32_t aHeight)
{
  if (!mContent)
    return NS_OK;

  nsAutoString width, height;
  width.AppendInt(aWidth);
  height.AppendInt(aHeight);

  nsCOMPtr<nsIContent> content = mContent;

  // We only want to pass aNotify=true to SetAttr once, but must make sure
  // we pass it when a value is being changed.  Thus, we check if the height
  // is the same and if so, pass true when setting the width.
  bool heightSame = content->AttrValueIs(kNameSpaceID_None, nsGkAtoms::height, height, eCaseMatters);

  content->SetAttr(kNameSpaceID_None, nsGkAtoms::width, width, heightSame);
  content->SetAttr(kNameSpaceID_None, nsGkAtoms::height, height, true);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAutoPosition(bool* aShouldAutoPosition)
{
  *aShouldAutoPosition = true;

  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    *aShouldAutoPosition = menuPopupFrame->GetAutoPosition();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::SetAutoPosition(bool aShouldAutoPosition)
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
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
nsPopupBoxObject::SetConsumeRollupEvent(uint32_t aConsume)
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

  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
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

  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  nsIContent* triggerContent = nsMenuPopupFrame::GetTriggerContent(menuPopupFrame);
  if (triggerContent)
    CallQueryInterface(triggerContent, aTriggerNode);

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAnchorNode(nsIDOMElement** aAnchor)
{
  *aAnchor = nullptr;

  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
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
  DOMRect* rect = new DOMRect(mContent);

  NS_ADDREF(*aRect = rect);

  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame)
    return NS_OK;

  // Return an empty rectangle if the popup is not open.
  nsPopupState state = menuPopupFrame->PopupState();
  if (state != ePopupOpen && state != ePopupOpenAndVisible)
    return NS_OK;

  nsView* view = menuPopupFrame->GetView();
  if (view) {
    nsIWidget* widget = view->GetWidget();
    if (widget) {
      nsIntRect screenRect;
      widget->GetScreenBounds(screenRect);

      int32_t pp = menuPopupFrame->PresContext()->AppUnitsPerDevPixel();
      rect->SetLayoutRect(screenRect.ToAppUnits(pp));
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAlignmentPosition(nsAString& positionStr)
{
  positionStr.Truncate();

  // This needs to flush layout.
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(true));
  if (!menuPopupFrame)
    return NS_OK;

  int8_t position = menuPopupFrame->GetAlignmentPosition();
  switch (position) {
    case POPUPPOSITION_AFTERSTART:
      positionStr.AssignLiteral("after_start");
      break;
    case POPUPPOSITION_AFTEREND:
      positionStr.AssignLiteral("after_end");
      break;
    case POPUPPOSITION_BEFORESTART:
      positionStr.AssignLiteral("before_start");
      break;
    case POPUPPOSITION_BEFOREEND:
      positionStr.AssignLiteral("before_end");
      break;
    case POPUPPOSITION_STARTBEFORE:
      positionStr.AssignLiteral("start_before");
      break;
    case POPUPPOSITION_ENDBEFORE:
      positionStr.AssignLiteral("end_before");
      break;
    case POPUPPOSITION_STARTAFTER:
      positionStr.AssignLiteral("start_after");
      break;
    case POPUPPOSITION_ENDAFTER:
      positionStr.AssignLiteral("end_after");
      break;
    case POPUPPOSITION_OVERLAP:
      positionStr.AssignLiteral("overlap");
      break;
    case POPUPPOSITION_AFTERPOINTER:
      positionStr.AssignLiteral("after_pointer");
      break;
    default:
      // Leave as an empty string.
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPopupBoxObject::GetAlignmentOffset(int32_t *aAlignmentOffset)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame)
    return NS_OK;

  int32_t pp = mozilla::AppUnitsPerCSSPixel();
  // Note that the offset might be along either the X or Y axis, but for the
  // sake of simplicity we use a point with only the X axis set so we can
  // use ToNearestPixels().
  nsPoint appOffset(menuPopupFrame->GetAlignmentOffset(), 0);
  nsIntPoint popupOffset = appOffset.ToNearestPixels(pp);
  *aAlignmentOffset = popupOffset.x;
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
