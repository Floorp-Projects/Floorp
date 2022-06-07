/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XULResizerElement.h"
#include "mozilla/dom/XULResizerElementBinding.h"

#include "mozilla/EventDispatcher.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "nsICSSDeclaration.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsStyledElement.h"

namespace mozilla::dom {

nsXULElement* NS_NewXULResizerElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo) {
  RefPtr<mozilla::dom::NodeInfo> nodeInfo(aNodeInfo);
  auto* nim = nodeInfo->NodeInfoManager();
  return new (nim) XULResizerElement(nodeInfo.forget());
}

static bool GetEventPoint(const WidgetGUIEvent* aEvent,
                          LayoutDeviceIntPoint& aPoint) {
  NS_ENSURE_TRUE(aEvent, false);

  const WidgetTouchEvent* touchEvent = aEvent->AsTouchEvent();
  if (touchEvent) {
    // return false if there is more than one touch on the page, or if
    // we can't find a touch point
    if (touchEvent->mTouches.Length() != 1) {
      return false;
    }

    const dom::Touch* touch = touchEvent->mTouches.SafeElementAt(0);
    if (!touch) {
      return false;
    }
    aPoint = touch->mRefPoint;
  } else {
    aPoint = aEvent->mRefPoint;
  }
  return true;
}

JSObject* XULResizerElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return XULResizerElement_Binding::Wrap(aCx, this, aGivenProto);
}

XULResizerElement::Direction XULResizerElement::GetDirection() {
  static const mozilla::dom::Element::AttrValuesArray strings[] = {
      // clang-format off
     nsGkAtoms::topleft,    nsGkAtoms::top,    nsGkAtoms::topright,
     nsGkAtoms::left,                          nsGkAtoms::right,
     nsGkAtoms::bottomleft, nsGkAtoms::bottom, nsGkAtoms::bottomright,
     nsGkAtoms::bottomstart,                   nsGkAtoms::bottomend,
     nullptr
      // clang-format on
  };

  static const Direction directions[] = {
      // clang-format off
     {-1, -1}, {0, -1}, {1, -1},
     {-1,  0},          {1,  0},
     {-1,  1}, {0,  1}, {1,  1},
     {-1,  1},          {1,  1}
      // clang-format on
  };

  const auto* frame = GetPrimaryFrame();
  if (!frame) {
    return directions[0];  // default: topleft
  }

  int32_t index =
      FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::dir, strings, eCaseMatters);
  if (index < 0) {
    return directions[0];  // default: topleft
  }

  if (index >= 8) {
    // Directions 8 and higher are RTL-aware directions and should reverse the
    // horizontal component if RTL.
    auto wm = frame->GetWritingMode();
    if (wm.IsPhysicalRTL()) {
      Direction direction = directions[index];
      direction.mHorizontal *= -1;
      return direction;
    }
  }

  return directions[index];
}

nsresult XULResizerElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
    PostHandleEventInternal(aVisitor);
  }
  return nsXULElement::PostHandleEvent(aVisitor);
}

void XULResizerElement::PostHandleEventInternal(
    EventChainPostVisitor& aVisitor) {
  bool doDefault = true;
  const WidgetEvent& event = *aVisitor.mEvent;
  switch (event.mMessage) {
    case eTouchStart:
    case eMouseDown: {
      if (event.mClass == eTouchEventClass ||
          (event.mClass == eMouseEventClass &&
           event.AsMouseEvent()->mButton == MouseButton::ePrimary)) {
        nsIContent* contentToResize = GetContentToResize();
        if (!contentToResize) {
          break;  // don't do anything if there's nothing to resize
        }
        auto* guiEvent = event.AsGUIEvent();
        nsIFrame* frame = contentToResize->GetPrimaryFrame();
        if (!frame) {
          break;
        }
        // cache the content rectangle for the frame to resize
        // GetScreenRectInAppUnits returns the border box rectangle, so
        // adjust to get the desired content rectangle.
        nsRect rect = frame->GetScreenRectInAppUnits();
        if (frame->StylePosition()->mBoxSizing == StyleBoxSizing::Content) {
          rect.Deflate(frame->GetUsedBorderAndPadding());
        }

        mMouseDownRect = LayoutDeviceIntRect::FromAppUnitsToNearest(
            rect, frame->PresContext()->AppUnitsPerDevPixel());

        // remember current mouse coordinates
        LayoutDeviceIntPoint refPoint;
        if (!GetEventPoint(guiEvent, refPoint)) {
          break;
        }
        mMouseDownPoint = refPoint + guiEvent->mWidget->WidgetToScreenOffset();
        mTrackingMouseMove = true;
        PresShell::SetCapturingContent(this, CaptureFlags::IgnoreAllowedState);
        doDefault = false;
      }
    } break;

    case eTouchMove:
    case eMouseMove: {
      if (mTrackingMouseMove) {
        nsCOMPtr<nsIContent> contentToResize = GetContentToResize();
        if (!contentToResize) {
          break;  // don't do anything if there's nothing to resize
        }
        nsIFrame* frame = contentToResize->GetPrimaryFrame();
        if (!frame) {
          break;
        }

        // both MouseMove and direction are negative when pointing to the
        // top and left, and positive when pointing to the bottom and right

        // retrieve the offset of the mousemove event relative to the mousedown.
        // The difference is how much the resize needs to be
        LayoutDeviceIntPoint refPoint;
        auto* guiEvent = event.AsGUIEvent();
        if (!GetEventPoint(guiEvent, refPoint)) {
          break;
        }
        LayoutDeviceIntPoint screenPoint =
            refPoint + guiEvent->mWidget->WidgetToScreenOffset();
        LayoutDeviceIntPoint mouseMove(screenPoint - mMouseDownPoint);

        // Determine which direction to resize by checking the dir attribute.
        // For windows and menus, ensure that it can be resized in that
        // direction.
        Direction direction = GetDirection();

        LayoutDeviceIntRect rect = mMouseDownRect;

        // Check if there are any size constraints on this window.
        AdjustDimensions(&rect.x, &rect.width, mouseMove.x,
                         direction.mHorizontal);
        AdjustDimensions(&rect.y, &rect.height, mouseMove.y,
                         direction.mVertical);

        // convert the rectangle into css pixels. When changing the size in a
        // direction, don't allow the new size to be less that the resizer's
        // size. This ensures that content isn't resized too small as to make
        // the resizer invisible.
        nsRect appUnitsRect = ToAppUnits(
            rect.ToUnknownRect(), frame->PresContext()->AppUnitsPerDevPixel());
        if (auto* resizerFrame = GetPrimaryFrame()) {
          nsRect frameRect = resizerFrame->GetRect();
          if (appUnitsRect.width < frameRect.width && mouseMove.x)
            appUnitsRect.width = frameRect.width;
          if (appUnitsRect.height < frameRect.height && mouseMove.y)
            appUnitsRect.height = frameRect.height;
        }
        nsIntRect cssRect = appUnitsRect.ToInsidePixels(AppUnitsPerCSSPixel());

        SizeInfo sizeInfo, originalSizeInfo;
        sizeInfo.width.AppendInt(cssRect.width);
        sizeInfo.height.AppendInt(cssRect.height);
        ResizeContent(contentToResize, direction, sizeInfo, &originalSizeInfo);
        MaybePersistOriginalSize(contentToResize, originalSizeInfo);

        doDefault = false;
      }
    } break;

    case eMouseClick: {
      auto* mouseEvent = event.AsMouseEvent();
      if (mouseEvent->IsLeftClickEvent()) {
        // Execute the oncommand event handler.
        nsContentUtils::DispatchXULCommand(
            this, false, nullptr, nullptr, mouseEvent->IsControl(),
            mouseEvent->IsAlt(), mouseEvent->IsShift(), mouseEvent->IsMeta(),
            mouseEvent->mInputSource, mouseEvent->mButton);
      }
    } break;

    case eTouchEnd:
    case eMouseUp: {
      if (event.mClass == eTouchEventClass ||
          (event.mClass == eMouseEventClass &&
           event.AsMouseEvent()->mButton == MouseButton::ePrimary)) {
        mTrackingMouseMove = false;
        PresShell::ReleaseCapturingContent();
        doDefault = false;
      }
    } break;

    case eMouseDoubleClick: {
      if (event.AsMouseEvent()->mButton == MouseButton::ePrimary) {
        if (nsIContent* contentToResize = GetContentToResize()) {
          RestoreOriginalSize(contentToResize);
        }
      }
    } break;

    default:
      break;
  }

  if (!doDefault) {
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
  }
}

nsIContent* XULResizerElement::GetContentToResize() const {
  if (!IsInComposedDoc()) {
    return nullptr;
  }
  // Return the parent, but skip over native anonymous content
  nsIContent* parent = GetParent();
  return parent ? parent->FindFirstNonChromeOnlyAccessContent() : nullptr;
}

/* static */
void XULResizerElement::AdjustDimensions(int32_t* aPos, int32_t* aSize,
                                         int32_t aMovement,
                                         int8_t aResizerDirection) {
  int32_t oldSize = *aSize;

  *aSize += aResizerDirection * aMovement;
  // use one as a minimum size or the element could disappear
  if (*aSize < 1) {
    *aSize = 1;
  }

  // For left and top resizers, the window must be moved left by the same
  // amount that the window was resized.
  if (aResizerDirection == -1) {
    *aPos += oldSize - *aSize;
  }
}

/* static */
void XULResizerElement::ResizeContent(nsIContent* aContent,
                                      const Direction& aDirection,
                                      const SizeInfo& aSizeInfo,
                                      SizeInfo* aOriginalSizeInfo) {
  if (RefPtr<nsStyledElement> inlineStyleContent =
          nsStyledElement::FromNode(aContent)) {
    nsICSSDeclaration* decl = inlineStyleContent->Style();

    if (aOriginalSizeInfo) {
      decl->GetPropertyValue("width"_ns, aOriginalSizeInfo->width);
      decl->GetPropertyValue("height"_ns, aOriginalSizeInfo->height);
    }

    // only set the property if the element could have changed in that
    // direction
    if (aDirection.mHorizontal) {
      nsAutoCString widthstr(aSizeInfo.width);
      if (!widthstr.IsEmpty() &&
          !Substring(widthstr, widthstr.Length() - 2, 2).EqualsLiteral("px"))
        widthstr.AppendLiteral("px");
      decl->SetProperty("width"_ns, widthstr, ""_ns, IgnoreErrors());
    }
    if (aDirection.mVertical) {
      nsAutoCString heightstr(aSizeInfo.height);
      if (!heightstr.IsEmpty() &&
          !Substring(heightstr, heightstr.Length() - 2, 2).EqualsLiteral("px"))
        heightstr.AppendLiteral("px");
      decl->SetProperty("height"_ns, heightstr, ""_ns, IgnoreErrors());
    }
  }
}

/* static */
void XULResizerElement::MaybePersistOriginalSize(nsIContent* aContent,
                                                 const SizeInfo& aSizeInfo) {
  nsresult rv;
  aContent->GetProperty(nsGkAtoms::_moz_original_size, &rv);
  if (rv != NS_PROPTABLE_PROP_NOT_THERE) {
    return;
  }

  UniquePtr<SizeInfo> sizeInfo(new SizeInfo(aSizeInfo));
  rv = aContent->SetProperty(
      nsGkAtoms::_moz_original_size, sizeInfo.get(),
      nsINode::DeleteProperty<XULResizerElement::SizeInfo>);
  if (NS_SUCCEEDED(rv)) {
    Unused << sizeInfo.release();
  }
}

/* static */
void XULResizerElement::RestoreOriginalSize(nsIContent* aContent) {
  nsresult rv;
  SizeInfo* sizeInfo = static_cast<SizeInfo*>(
      aContent->GetProperty(nsGkAtoms::_moz_original_size, &rv));
  if (NS_FAILED(rv)) {
    return;
  }

  NS_ASSERTION(sizeInfo, "We set a null sizeInfo!?");
  Direction direction = {1, 1};
  ResizeContent(aContent, direction, *sizeInfo, nullptr);
  aContent->RemoveProperty(nsGkAtoms::_moz_original_size);
}

}  // namespace mozilla::dom
