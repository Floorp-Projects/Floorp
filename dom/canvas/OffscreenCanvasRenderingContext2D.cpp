/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OffscreenCanvasRenderingContext2D.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/OffscreenCanvasRenderingContext2DBinding.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/ServoCSSParser.h"
#include "gfxPlatform.h"
#include "gfxTextRun.h"

using namespace mozilla;

namespace mozilla::dom {

class OffscreenCanvasShutdownObserver final {
  NS_INLINE_DECL_REFCOUNTING(OffscreenCanvasShutdownObserver)

 public:
  explicit OffscreenCanvasShutdownObserver(
      OffscreenCanvasRenderingContext2D* aOwner)
      : mOwner(aOwner) {}

  void OnShutdown() {
    if (mOwner) {
      mOwner->OnShutdown();
      mOwner = nullptr;
    }
  }

  void ClearOwner() { mOwner = nullptr; }

 private:
  ~OffscreenCanvasShutdownObserver() = default;

  OffscreenCanvasRenderingContext2D* mOwner;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(OffscreenCanvasRenderingContext2D,
                                   CanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(OffscreenCanvasRenderingContext2D)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(CanvasRenderingContext2D)

// Need to use NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_INHERITED
// and dummy trace since we're missing some _SKIPPABLE_ macros without
// SCRIPT_HOLDER.
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    OffscreenCanvasRenderingContext2D, CanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(OffscreenCanvasRenderingContext2D)
  return tmp->HasKnownLiveWrapper();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_IMPL_ADDREF_INHERITED(OffscreenCanvasRenderingContext2D,
                         CanvasRenderingContext2D)
NS_IMPL_RELEASE_INHERITED(OffscreenCanvasRenderingContext2D,
                          CanvasRenderingContext2D)

OffscreenCanvasRenderingContext2D::OffscreenCanvasRenderingContext2D(
    layers::LayersBackend aCompositorBackend)
    : CanvasRenderingContext2D(aCompositorBackend) {}

OffscreenCanvasRenderingContext2D::~OffscreenCanvasRenderingContext2D() =
    default;

JSObject* OffscreenCanvasRenderingContext2D::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return OffscreenCanvasRenderingContext2D_Binding::Wrap(aCx, this,
                                                         aGivenProto);
}

nsIGlobalObject* OffscreenCanvasRenderingContext2D::GetParentObject() const {
  return mOffscreenCanvas->GetOwnerGlobal();
}

NS_IMETHODIMP OffscreenCanvasRenderingContext2D::InitializeWithDrawTarget(
    nsIDocShell* aShell, NotNull<gfx::DrawTarget*> aTarget) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

static nsAutoCString FamilyListToString(
    const StyleFontFamilyList& aFamilyList) {
  return StringJoin(","_ns, aFamilyList.list.AsSpan(),
                    [](nsACString& dst, const StyleSingleFontFamily& name) {
                      name.AppendToString(dst);
                    });
}

static void SerializeFontForCanvas(const StyleFontFamilyList& aList,
                                   const gfxFontStyle& aStyle,
                                   nsACString& aUsedFont) {
  // Re-serialize the font shorthand as required by the canvas spec.
  aUsedFont.Truncate();

  if (aStyle.style.IsItalic()) {
    aUsedFont.Append("italic ");
  } else if (aStyle.style.IsOblique()) {
    aUsedFont.Append("oblique ");
    // Include the angle if it is not the default font-style:oblique value.
    if (aStyle.style != FontSlantStyle::Oblique()) {
      aUsedFont.AppendFloat(aStyle.style.ObliqueAngle());
      aUsedFont.Append("deg ");
    }
  }

  // font-weight is serialized as a number
  if (!aStyle.weight.IsNormal()) {
    aUsedFont.AppendFloat(aStyle.weight.ToFloat());
  }

  // font-stretch is serialized using CSS Fonts 3 keywords, not percentages.
  if (!aStyle.stretch.IsNormal()) {
    if (aStyle.stretch == FontStretch::UltraCondensed()) {
      aUsedFont.Append("ultra-condensed ");
    } else if (aStyle.stretch == FontStretch::ExtraCondensed()) {
      aUsedFont.Append("extra-condensed ");
    } else if (aStyle.stretch == FontStretch::Condensed()) {
      aUsedFont.Append("condensed ");
    } else if (aStyle.stretch == FontStretch::SemiCondensed()) {
      aUsedFont.Append("semi-condensed ");
    } else if (aStyle.stretch == FontStretch::SemiExpanded()) {
      aUsedFont.Append("semi-expanded ");
    } else if (aStyle.stretch == FontStretch::Expanded()) {
      aUsedFont.Append("expanded ");
    } else if (aStyle.stretch == FontStretch::ExtraExpanded()) {
      aUsedFont.Append("extra-expanded ");
    } else if (aStyle.stretch == FontStretch::UltraExpanded()) {
      aUsedFont.Append("ultra-expanded ");
    }
  }

  // Serialize the computed (not specified) size, and the family name(s).
  aUsedFont.AppendFloat(aStyle.size);
  aUsedFont.Append("px ");
  aUsedFont.Append(FamilyListToString(aList));
}

bool OffscreenCanvasRenderingContext2D::SetFontInternal(const nsACString& aFont,
                                                        ErrorResult& aError) {
  // In the OffscreenCanvas case we don't have the context necessary to call
  // GetFontStyleForServo(), as we do in the main-thread canvas context, so
  // instead we borrow ParseFontShorthandForMatching to parse the attribute.
  float stretch = FontStretch::Normal().Percentage(),
        weight = FontWeight::Normal().ToFloat(), size = 10.0;
  StyleComputedFontStyleDescriptor style(
      StyleComputedFontStyleDescriptor::Normal());
  StyleFontFamilyList list;
  if (!ServoCSSParser::ParseFontShorthandForMatching(
          aFont, nullptr, list, style, stretch, weight, &size)) {
    return false;
  }

  gfxFontStyle fontStyle;
  fontStyle.size = size;
  fontStyle.weight = FontWeight(weight);
  fontStyle.stretch = FontStretch::FromStyle(stretch);
  switch (style.tag) {
    case StyleComputedFontStyleDescriptor::Tag::Normal:
      fontStyle.style = FontSlantStyle::Normal();
      break;
    case StyleComputedFontStyleDescriptor::Tag::Italic:
      fontStyle.style = FontSlantStyle::Italic();
      break;
    case StyleComputedFontStyleDescriptor::Tag::Oblique:
      fontStyle.style = FontSlantStyle::Oblique(style.AsOblique()._0);
      break;
  }

  // TODO: Get a userFontSet from the Worker and pass to the fontGroup
  // TODO: Should we be passing a language? Where from?
  // TODO: Cache fontGroups in the Worker (use an nsFontCache?)
  gfxFontGroup* fontGroup =
      gfxPlatform::GetPlatform()->CreateFontGroup(nullptr,  // aPresContext
                                                  list,     // aFontFamilyList
                                                  &fontStyle,  // aStyle
                                                  nullptr,     // aLanguage
                                                  false,    // aExplicitLanguage
                                                  nullptr,  // aTextPerf
                                                  nullptr,  // aUserFontSet
                                                  1.0);     // aDevToCssSize
  CurrentState().fontGroup = fontGroup;
  SerializeFontForCanvas(list, fontStyle, CurrentState().font);
  CurrentState().fontFont =
      nsFont(StyleFontFamily{list, false, false},
             StyleCSSPixelLength::FromPixels(float(fontStyle.size)));
  CurrentState().fontLanguage = nullptr;
  CurrentState().fontExplicitLanguage = false;
  return true;
}

void OffscreenCanvasRenderingContext2D::AddShutdownObserver() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    // We may be using OffscreenCanvas on the main thread.
    CanvasRenderingContext2D::AddShutdownObserver();
    return;
  }

  mOffscreenShutdownObserver =
      MakeAndAddRef<OffscreenCanvasShutdownObserver>(this);
  mWorkerRef = WeakWorkerRef::Create(
      workerPrivate,
      [observer = mOffscreenShutdownObserver] { observer->OnShutdown(); });
}

void OffscreenCanvasRenderingContext2D::RemoveShutdownObserver() {
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    // We may be using OffscreenCanvas on the main thread.
    CanvasRenderingContext2D::RemoveShutdownObserver();
    return;
  }

  if (mOffscreenShutdownObserver) {
    mOffscreenShutdownObserver->ClearOwner();
  }
  mOffscreenShutdownObserver = nullptr;
  mWorkerRef = nullptr;
}

void OffscreenCanvasRenderingContext2D::OnShutdown() {
  if (mOffscreenShutdownObserver) {
    mOffscreenShutdownObserver->ClearOwner();
    mOffscreenShutdownObserver = nullptr;
  }

  CanvasRenderingContext2D::OnShutdown();
}

void OffscreenCanvasRenderingContext2D::Commit(ErrorResult& aRv) {
  if (!mOffscreenCanvas->IsTransferredFromElement()) {
    return;
  }

  mOffscreenCanvas->CommitFrameToCompositor();
}

void OffscreenCanvasRenderingContext2D::AddZoneWaitingForGC() {
  JSObject* wrapper = GetWrapperPreserveColor();
  if (wrapper) {
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(
        JS::GetObjectZone(wrapper));
  }
}

void OffscreenCanvasRenderingContext2D::AddAssociatedMemory() {
  JSObject* wrapper = GetWrapperMaybeDead();
  if (wrapper) {
    JS::AddAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                            JS::MemoryUse::DOMBinding);
  }
}

void OffscreenCanvasRenderingContext2D::RemoveAssociatedMemory() {
  JSObject* wrapper = GetWrapperMaybeDead();
  if (wrapper) {
    JS::RemoveAssociatedMemory(wrapper, BindingJSObjectMallocBytes(this),
                               JS::MemoryUse::DOMBinding);
  }
}

size_t BindingJSObjectMallocBytes(OffscreenCanvasRenderingContext2D* aContext) {
  gfx::IntSize size = aContext->GetSize();

  // TODO: Bug 1552137: No memory will be allocated if either dimension is
  // greater than gfxPrefs::gfx_canvas_max_size(). We should check this here
  // too.

  CheckedInt<uint32_t> bytes =
      CheckedInt<uint32_t>(size.width) * size.height * 4;
  if (!bytes.isValid()) {
    return 0;
  }

  return bytes.value();
}

}  // namespace mozilla::dom
