/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIRootBox.h"
#include "nsIPresShell.h"
#include "nsIContent.h"
#include "nsNameSpaceManager.h"
#include "nsGkAtoms.h"
#include "nsMenuPopupFrame.h"
#include "nsView.h"
#include "mozilla/AppUnits.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/PopupBoxObject.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/PopupBoxObjectBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(PopupBoxObject, BoxObject)
NS_IMPL_RELEASE_INHERITED(PopupBoxObject, BoxObject)
NS_INTERFACE_MAP_BEGIN(PopupBoxObject)
NS_INTERFACE_MAP_END_INHERITING(BoxObject)

PopupBoxObject::PopupBoxObject()
{
}

PopupBoxObject::~PopupBoxObject()
{
}

nsIContent* PopupBoxObject::GetParentObject() const
{
  return BoxObject::GetParentObject();
}

JSObject* PopupBoxObject::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PopupBoxObjectBinding::Wrap(aCx, this, aGivenProto);
}

nsPopupSetFrame*
PopupBoxObject::GetPopupSetFrame()
{
  nsIRootBox* rootBox = nsIRootBox::GetRootBox(GetPresShell(false));
  if (!rootBox)
    return nullptr;

  return rootBox->GetPopupSetFrame();
}

void
PopupBoxObject::HidePopup(bool aCancel)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    pm->HidePopup(mContent, false, true, false, aCancel);
  }
}

void
PopupBoxObject::ShowPopup(Element* aAnchorElement,
                          Element& aPopupElement,
                          int32_t aXPos, int32_t aYPos,
                          const nsAString& aPopupType,
                          const nsAString& aAnchorAlignment,
                          const nsAString& aPopupAlignment)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));
    nsAutoString popupType(aPopupType);
    nsAutoString anchor(aAnchorAlignment);
    nsAutoString align(aPopupAlignment);
    pm->ShowPopupWithAnchorAlign(mContent, anchorContent, anchor, align,
                                 aXPos, aYPos,
                                 popupType.EqualsLiteral("context"));
  }
}

void
PopupBoxObject::OpenPopup(Element* aAnchorElement,
                          const nsAString& aPosition,
                          int32_t aXPos, int32_t aYPos,
                          bool aIsContextMenu,
                          bool aAttributesOverride,
                          Event* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));
    pm->ShowPopup(mContent, anchorContent, aPosition, aXPos, aYPos,
                  aIsContextMenu, aAttributesOverride, false, aTriggerEvent);
  }
}

void
PopupBoxObject::OpenPopupAtScreen(int32_t aXPos, int32_t aYPos,
                                  bool aIsContextMenu,
                                  Event* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent)
    pm->ShowPopupAtScreen(mContent, aXPos, aYPos, aIsContextMenu, aTriggerEvent);
}

void
PopupBoxObject::OpenPopupAtScreenRect(const nsAString& aPosition,
                                      int32_t aXPos, int32_t aYPos,
                                      int32_t aWidth, int32_t aHeight,
                                      bool aIsContextMenu,
                                      bool aAttributesOverride,
                                      Event* aTriggerEvent)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && mContent) {
    pm->ShowPopupAtScreenRect(mContent, aPosition,
                              nsIntRect(aXPos, aYPos, aWidth, aHeight),
                              aIsContextMenu, aAttributesOverride, aTriggerEvent);
  }
}

void
PopupBoxObject::MoveTo(int32_t aLeft, int32_t aTop)
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    menuPopupFrame->MoveTo(CSSIntPoint(aLeft, aTop), true);
  }
}

void
PopupBoxObject::MoveToAnchor(Element* aAnchorElement,
                             const nsAString& aPosition,
                             int32_t aXPos, int32_t aYPos,
                             bool aAttributesOverride)
{
  if (mContent) {
    nsCOMPtr<nsIContent> anchorContent(do_QueryInterface(aAnchorElement));

    nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(mContent->GetPrimaryFrame());
    if (menuPopupFrame && menuPopupFrame->IsVisible()) {
      menuPopupFrame->MoveToAnchor(anchorContent, aPosition, aXPos, aYPos, aAttributesOverride);
    }
  }
}

void
PopupBoxObject::SizeTo(int32_t aWidth, int32_t aHeight)
{
  if (!mContent)
    return;

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
}

bool
PopupBoxObject::AutoPosition()
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    return menuPopupFrame->GetAutoPosition();
  }
  return true;
}

void
PopupBoxObject::SetAutoPosition(bool aShouldAutoPosition)
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    menuPopupFrame->SetAutoPosition(aShouldAutoPosition);
  }
}

void
PopupBoxObject::EnableRollup(bool aShouldRollup)
{
  // this does nothing now
}

void
PopupBoxObject::SetConsumeRollupEvent(uint32_t aConsume)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    menuPopupFrame->SetConsumeRollupEvent(aConsume);
  }
}

void
PopupBoxObject::EnableKeyboardNavigator(bool aEnableKeyboardNavigator)
{
  if (!mContent)
    return;

  // Use ignorekeys="true" on the popup instead of using this function.
  if (aEnableKeyboardNavigator)
    mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::ignorekeys, true);
  else
    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::ignorekeys,
                      NS_LITERAL_STRING("true"), true);
}

void
PopupBoxObject::GetPopupState(nsString& aState)
{
  // set this here in case there's no frame for the popup
  aState.AssignLiteral("closed");

  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (menuPopupFrame) {
    switch (menuPopupFrame->PopupState()) {
      case ePopupShown:
        aState.AssignLiteral("open");
        break;
      case ePopupShowing:
      case ePopupPositioning:
      case ePopupOpening:
      case ePopupVisible:
        aState.AssignLiteral("showing");
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
}

nsINode*
PopupBoxObject::GetTriggerNode() const
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  return nsMenuPopupFrame::GetTriggerContent(menuPopupFrame);
}

Element*
PopupBoxObject::GetAnchorNode() const
{
  nsMenuPopupFrame *menuPopupFrame = mContent ? do_QueryFrame(mContent->GetPrimaryFrame()) : nullptr;
  if (!menuPopupFrame) {
    return nullptr;
  }

  nsIContent* anchor = menuPopupFrame->GetAnchor();
  return anchor && anchor->IsElement() ? anchor->AsElement() : nullptr;
}

already_AddRefed<DOMRect>
PopupBoxObject::GetOuterScreenRect()
{
  RefPtr<DOMRect> rect = new DOMRect(mContent);

  // Return an empty rectangle if the popup is not open.
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame || !menuPopupFrame->IsOpen()) {
    return rect.forget();
  }

  nsView* view = menuPopupFrame->GetView();
  if (view) {
    nsIWidget* widget = view->GetWidget();
    if (widget) {
      LayoutDeviceIntRect screenRect = widget->GetScreenBounds();

      int32_t pp = menuPopupFrame->PresContext()->AppUnitsPerDevPixel();
      rect->SetLayoutRect(LayoutDeviceIntRect::ToAppUnits(screenRect, pp));
    }
  }
  return rect.forget();
}

void
PopupBoxObject::GetAlignmentPosition(nsString& positionStr)
{
  positionStr.Truncate();

  // This needs to flush layout.
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(true));
  if (!menuPopupFrame)
    return;

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
    case POPUPPOSITION_SELECTION:
      positionStr.AssignLiteral("selection");
      break;
    default:
      // Leave as an empty string.
      break;
  }
}

int32_t
PopupBoxObject::AlignmentOffset()
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (!menuPopupFrame)
    return 0;

  int32_t pp = mozilla::AppUnitsPerCSSPixel();
  // Note that the offset might be along either the X or Y axis, but for the
  // sake of simplicity we use a point with only the X axis set so we can
  // use ToNearestPixels().
  nsPoint appOffset(menuPopupFrame->GetAlignmentOffset(), 0);
  nsIntPoint popupOffset = appOffset.ToNearestPixels(pp);
  return popupOffset.x;
}

void
PopupBoxObject::SetConstraintRect(dom::DOMRectReadOnly& aRect)
{
  nsMenuPopupFrame *menuPopupFrame = do_QueryFrame(GetFrame(false));
  if (menuPopupFrame) {
    menuPopupFrame->SetOverrideConstraintRect(
      LayoutDeviceIntRect::Truncate(aRect.Left(), aRect.Top(), aRect.Width(), aRect.Height()));
  }
}

} // namespace dom
} // namespace mozilla

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPopupBoxObject(nsIBoxObject** aResult)
{
  *aResult = new mozilla::dom::PopupBoxObject();
  NS_ADDREF(*aResult);
  return NS_OK;
}
