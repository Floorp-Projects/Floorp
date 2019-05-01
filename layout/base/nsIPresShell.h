/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a presentation of a document, part 2 */

#ifndef nsIPresShell_h___
#define nsIPresShell_h___

#include "mozilla/PresShellForwards.h"

#include "mozilla/ArenaObjectID.h"
#include "mozilla/EventForwards.h"
#include "mozilla/FlushType.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/ScrollTypes.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/WeakPtr.h"
#include "FrameMetrics.h"
#include "GeckoProfiler.h"
#include "gfxPoint.h"
#include "nsDOMNavigationTiming.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsISelectionController.h"
#include "nsQueryFrame.h"
#include "nsStringFwd.h"
#include "nsCoord.h"
#include "nsColor.h"
#include "nsFrameManager.h"
#include "nsRect.h"
#include "nsRegionFwd.h"
#include <stdio.h>  // for FILE definition
#include "nsChangeHint.h"
#include "nsRefPtrHashtable.h"
#include "nsClassHashtable.h"
#include "nsPresArena.h"
#include "nsIImageLoadingContent.h"
#include "nsMargin.h"
#include "nsFrameState.h"
#include "nsStubDocumentObserver.h"
#include "nsCOMArray.h"
#include "Units.h"

#ifdef MOZ_REFLOW_PERF
class ReflowCountMgr;
#endif

class gfxContext;
struct nsCallbackEventRequest;
class nsDocShell;
class nsIFrame;
class nsPresContext;
class nsWindowSizes;
class nsViewManager;
class nsView;
class nsIPageSequenceFrame;
class nsCanvasFrame;
class nsCaret;
namespace mozilla {
class AccessibleCaretEventHub;
class OverflowChangedTracker;
class StyleSheet;
}  // namespace mozilla
class nsFrameSelection;
class nsFrameManager;
class nsILayoutHistoryState;
class nsIReflowCallback;
class nsCSSFrameConstructor;
template <class E>
class nsCOMArray;
class AutoWeakFrame;
class MobileViewportManager;
class WeakFrame;
class nsIScrollableFrame;
class nsDisplayList;
class nsDisplayListBuilder;
class nsPIDOMWindowOuter;
struct nsPoint;
class nsINode;
struct nsRect;
class nsRegion;
class nsRefreshDriver;
class nsAutoCauseReflowNotifier;
class nsARefreshObserver;
class nsAPostRefreshObserver;
#ifdef ACCESSIBILITY
class nsAccessibilityService;
namespace mozilla {
namespace a11y {
class DocAccessible;
}  // namespace a11y
}  // namespace mozilla
#endif
class nsITimer;

namespace mozilla {
class EventStates;

namespace dom {
class Element;
class Event;
class Document;
class HTMLSlotElement;
class Touch;
class Selection;
class ShadowRoot;
}  // namespace dom

namespace layout {
class ScrollAnchorContainer;
}  // namespace layout

namespace layers {
class LayerManager;
}  // namespace layers

namespace gfx {
class SourceSurface;
}  // namespace gfx
}  // namespace mozilla

// b7b89561-4f03-44b3-9afa-b47e7f313ffb
#define NS_IPRESSHELL_IID                            \
  {                                                  \
    0xb7b89561, 0x4f03, 0x44b3, {                    \
      0x9a, 0xfa, 0xb4, 0x7e, 0x7f, 0x31, 0x3f, 0xfb \
    }                                                \
  }

#undef NOISY_INTERRUPTIBLE_REFLOW

/**
 * Presentation shell interface. Presentation shells are the
 * controlling point for managing the presentation of a document. The
 * presentation shell holds a live reference to the document, the
 * presentation context, the style manager, the style set and the root
 * frame. <p>
 *
 * When this object is Release'd, it will release the document, the
 * presentation context, the style manager, the style set and the root
 * frame.
 */

class nsIPresShell : public nsStubDocumentObserver {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRESSHELL_IID)

  nsIPresShell() = default;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPresShell, NS_IPRESSHELL_IID)

#endif /* nsIPresShell_h___ */
