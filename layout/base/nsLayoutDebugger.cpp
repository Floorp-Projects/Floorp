/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * implementation of interface that allows layout-debug extension access
 * to some internals of layout
 */

#include "nsILayoutDebugger.h"

#include "nsAttrValue.h"
#include "nsFrame.h"
#include "nsDisplayList.h"
#include "FrameLayerBuilder.h"
#include "nsPrintfCString.h"

#include <stdio.h>

using namespace mozilla;
using namespace mozilla::layers;

#ifdef DEBUG
class nsLayoutDebugger : public nsILayoutDebugger {
public:
  nsLayoutDebugger();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetShowFrameBorders(bool aEnable) override;

  NS_IMETHOD GetShowFrameBorders(bool* aResult) override;

  NS_IMETHOD SetShowEventTargetFrameBorder(bool aEnable) override;

  NS_IMETHOD GetShowEventTargetFrameBorder(bool* aResult) override;

protected:
  virtual ~nsLayoutDebugger();
};

nsresult
NS_NewLayoutDebugger(nsILayoutDebugger** aResult)
{
  MOZ_ASSERT(aResult, "null OUT ptr");
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsLayoutDebugger* it = new nsLayoutDebugger();
  return it->QueryInterface(NS_GET_IID(nsILayoutDebugger), (void**)aResult);
}

nsLayoutDebugger::nsLayoutDebugger()
{
}

nsLayoutDebugger::~nsLayoutDebugger()
{
}

NS_IMPL_ISUPPORTS(nsLayoutDebugger, nsILayoutDebugger)

NS_IMETHODIMP
nsLayoutDebugger::SetShowFrameBorders(bool aEnable)
{
  nsFrame::ShowFrameBorders(aEnable);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebugger::GetShowFrameBorders(bool* aResult)
{
  *aResult = nsFrame::GetShowFrameBorders();
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebugger::SetShowEventTargetFrameBorder(bool aEnable)
{
  nsFrame::ShowEventTargetFrameBorder(aEnable);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebugger::GetShowEventTargetFrameBorder(bool* aResult)
{
  *aResult = nsFrame::GetShowEventTargetFrameBorder();
  return NS_OK;
}

#endif

static std::ostream&
operator<<(std::ostream& os, const nsPrintfCString& rhs)
{
  os << rhs.get();
  return os;
}

static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   std::stringstream& aStream, uint32_t aIndent, bool aDumpHtml);

static void
PrintDisplayItemTo(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
                   std::stringstream& aStream, uint32_t aIndent, bool aDumpSublist, bool aDumpHtml)
{
  std::stringstream ss;

  if (!aDumpHtml) {
    for (uint32_t indent = 0; indent < aIndent; indent++) {
      aStream << "  ";
    }
  }
  nsAutoString contentData;
  nsIFrame* f = aItem->Frame();
#ifdef DEBUG_FRAME_DUMP
  f->GetFrameName(contentData);
#endif
  nsIContent* content = f->GetContent();
  if (content) {
    nsString tmp;
    if (content->GetID()) {
      content->GetID()->ToString(tmp);
      contentData.AppendLiteral(" id:");
      contentData.Append(tmp);
    }
    const nsAttrValue* classes = content->IsElement() ?
      content->AsElement()->GetClasses() : nullptr;
    if (classes) {
      classes->ToString(tmp);
      contentData.AppendLiteral(" class:");
      contentData.Append(tmp);
    }
  }
  bool snap;
  nsRect rect = aItem->GetBounds(aBuilder, &snap);
  nsRect layerRect = rect - (*aItem->GetAnimatedGeometryRoot())->GetOffsetToCrossDoc(aItem->ReferenceFrame());
  nsRect vis = aItem->GetPaintRect();
  nsRect component = aItem->GetComponentAlphaBounds(aBuilder);
  nsDisplayList* list = aItem->GetChildren();
  const DisplayItemClip& clip = aItem->GetClip();
  nsRegion opaque = aItem->GetOpaqueRegion(aBuilder, &snap);

#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml && aItem->Painted()) {
    nsCString string(aItem->Name());
    string.Append('-');
    string.AppendInt((uint64_t)aItem);
    aStream << nsPrintfCString("<a href=\"javascript:ViewImage('%s')\">", string.BeginReading());
  }
#endif

  aStream << nsPrintfCString("%s p=0x%p f=0x%p(%s) key=%d %sbounds(%d,%d,%d,%d) layerBounds(%d,%d,%d,%d) visible(%d,%d,%d,%d) componentAlpha(%d,%d,%d,%d) clip(%s) asr(%s) clipChain(%s)%s ref=0x%p agr=0x%p",
          aItem->Name(), aItem, (void*)f, NS_ConvertUTF16toUTF8(contentData).get(), aItem->GetPerFrameKey(),
          (aItem->ZIndex() ? nsPrintfCString("z=%d ", aItem->ZIndex()).get() : ""),
          rect.x, rect.y, rect.width, rect.height,
          layerRect.x, layerRect.y, layerRect.width, layerRect.height,
          vis.x, vis.y, vis.width, vis.height,
          component.x, component.y, component.width, component.height,
          clip.ToString().get(),
          ActiveScrolledRoot::ToString(aItem->GetActiveScrolledRoot()).get(),
          DisplayItemClipChain::ToString(aItem->GetClipChain()).get(),
          aItem->IsUniform(aBuilder) ? " uniform" : "",
          aItem->ReferenceFrame(), aItem->GetAnimatedGeometryRoot()->mFrame);

  for (auto iter = opaque.RectIter(); !iter.Done(); iter.Next()) {
    const nsRect& r = iter.Get();
    aStream << nsPrintfCString(" (opaque %d,%d,%d,%d)", r.x, r.y, r.width, r.height);
  }

  const auto& willChange = aItem->Frame()->StyleDisplay()->mWillChange;
  if (!willChange.IsEmpty()) {
    aStream << " (will-change=";
    for (size_t i = 0; i < willChange.Length(); i++) {
      if (i > 0) {
        aStream << ",";
      }
      nsDependentAtomString buffer(willChange[i]);
      aStream << NS_LossyConvertUTF16toASCII(buffer).get();
    }
    aStream << ")";
  }

  // Display item specific debug info
  aItem->WriteDebugInfo(aStream);

#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml && aItem->Painted()) {
    aStream << "</a>";
  }
#endif
  DisplayItemData* data = mozilla::FrameLayerBuilder::GetOldDataFor(aItem);
  if (data && data->GetLayer()) {
    if (aDumpHtml) {
      aStream << nsPrintfCString(" <a href=\"#%p\">layer=%p</a>", data->GetLayer(), data->GetLayer());
    } else {
      aStream << nsPrintfCString(" layer=0x%p", data->GetLayer());
    }
  }
#ifdef MOZ_DUMP_PAINTING
  if (aItem->GetType() == DisplayItemType::TYPE_MASK) {
    nsCString str;
    (static_cast<nsDisplayMask*>(aItem))->PrintEffects(str);
    aStream << str.get();
  }

  if (aItem->GetType() == DisplayItemType::TYPE_FILTER) {
    nsCString str;
    (static_cast<nsDisplayFilter*>(aItem))->PrintEffects(str);
    aStream << str.get();
  }
#endif
  aStream << "\n";
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml && aItem->Painted()) {
    nsCString string(aItem->Name());
    string.Append('-');
    string.AppendInt((uint64_t)aItem);
    aStream << nsPrintfCString("<br><img id=\"%s\">\n", string.BeginReading());
  }
#endif

  if (aDumpSublist && list) {
    PrintDisplayListTo(aBuilder, *list, aStream, aIndent+1, aDumpHtml);
  }
}

static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   std::stringstream& aStream, uint32_t aIndent, bool aDumpHtml)
{
  if (aDumpHtml) {
    aStream << "<ul>";
  }

  for (nsDisplayItem* i = aList.GetBottom(); i != nullptr; i = i->GetAbove()) {
    if (aDumpHtml) {
      aStream << "<li>";
    }
    PrintDisplayItemTo(aBuilder, i, aStream, aIndent, true, aDumpHtml);
    if (aDumpHtml) {
      aStream << "</li>";
    }
  }

  if (aDumpHtml) {
    aStream << "</ul>";
  }
}

void
nsFrame::PrintDisplayList(nsDisplayListBuilder* aBuilder,
                          const nsDisplayList& aList,
                          std::stringstream& aStream,
                          bool aDumpHtml)
{
  PrintDisplayListTo(aBuilder, aList, aStream, 0, aDumpHtml);
}

/**
 * The two functions below are intended to be called from a debugger.
 */
void
PrintDisplayItemToStdout(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
{
  std::stringstream stream;
  PrintDisplayItemTo(aBuilder, aItem, stream, 0, true, false);
  puts(stream.str().c_str());
}

void
PrintDisplayListToStdout(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList)
{
  std::stringstream stream;
  PrintDisplayListTo(aBuilder, aList, stream, 0, false);
  puts(stream.str().c_str());
}

#ifdef MOZ_DUMP_PAINTING
static void
PrintDisplayListSetItem(nsDisplayListBuilder* aBuilder,
                        const char* aItemName,
                        const nsDisplayList& aList,
                        std::stringstream& aStream,
                        bool aDumpHtml)
{
  if (aDumpHtml) {
    aStream << "<li>";
  }
  aStream << aItemName << "\n";
  PrintDisplayListTo(aBuilder, aList, aStream, 0, aDumpHtml);
  if (aDumpHtml) {
    aStream << "</li>";
  }
}

void
nsFrame::PrintDisplayListSet(nsDisplayListBuilder* aBuilder,
                             const nsDisplayListSet& aSet,
                             std::stringstream& aStream,
                             bool aDumpHtml)
{
  if (aDumpHtml) {
    aStream << "<ul>";
  }
  PrintDisplayListSetItem(aBuilder, "[BorderBackground]", *(aSet.BorderBackground()), aStream, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[BlockBorderBackgrounds]", *(aSet.BlockBorderBackgrounds()), aStream, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Floats]", *(aSet.Floats()), aStream, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[PositionedDescendants]", *(aSet.PositionedDescendants()), aStream, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Outlines]", *(aSet.Outlines()), aStream, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Content]", *(aSet.Content()), aStream, aDumpHtml);
  if (aDumpHtml) {
    aStream << "</ul>";
  }
}

#endif
