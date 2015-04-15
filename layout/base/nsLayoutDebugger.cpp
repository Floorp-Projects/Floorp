/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * implementation of interface that allows layout-debug extension access
 * to some internals of layout
 */

#include "nsILayoutDebugger.h"
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

  NS_IMETHOD GetContentSize(nsIDocument* aDocument,
                            int32_t* aSizeInBytesResult) override;

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult) override;

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult) override;

protected:
  virtual ~nsLayoutDebugger();
};

nsresult
NS_NewLayoutDebugger(nsILayoutDebugger** aResult)
{
  NS_PRECONDITION(aResult, "null OUT ptr");
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

NS_IMETHODIMP
nsLayoutDebugger::GetContentSize(nsIDocument* aDocument,
                                 int32_t* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLayoutDebugger::GetFrameSize(nsIPresShell* aPresentation,
                               int32_t* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLayoutDebugger::GetStyleSize(nsIPresShell* aPresentation,
                               int32_t* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}
#endif

std::ostream& operator<<(std::ostream& os, const nsPrintfCString& rhs) {
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
    if (content->GetClasses()) {
      content->GetClasses()->ToString(tmp);
      contentData.AppendLiteral(" class:");
      contentData.Append(tmp);
    }
  }
  bool snap;
  nsRect rect = aItem->GetBounds(aBuilder, &snap);
  nsRect layerRect = rect -
    nsLayoutUtils::GetAnimatedGeometryRootFor(aItem, aBuilder, nullptr)->
      GetOffsetToCrossDoc(aItem->ReferenceFrame());
  nscolor color;
  nsRect vis = aItem->GetVisibleRect();
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
  aStream << nsPrintfCString("%s p=0x%p f=0x%p(%s) %sbounds(%d,%d,%d,%d) layerBounds(%d,%d,%d,%d) visible(%d,%d,%d,%d) componentAlpha(%d,%d,%d,%d) clip(%s) %s",
          aItem->Name(), aItem, (void*)f, NS_ConvertUTF16toUTF8(contentData).get(),
          (aItem->ZIndex() ? nsPrintfCString("z=%d ", aItem->ZIndex()).get() : ""),
          rect.x, rect.y, rect.width, rect.height,
          layerRect.x, layerRect.y, layerRect.width, layerRect.height,
          vis.x, vis.y, vis.width, vis.height,
          component.x, component.y, component.width, component.height,
          clip.ToString().get(),
          aItem->IsUniform(aBuilder, &color) ? " uniform" : "");

  nsRegionRectIterator iter(opaque);
  for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
    aStream << nsPrintfCString(" (opaque %d,%d,%d,%d)", r->x, r->y, r->width, r->height);
  }

  if (aItem->ShouldFixToViewport(nullptr)) {
    aStream << " fixed";
  }

  if (aItem->Frame()->StyleDisplay()->mWillChange.Length() > 0) {
    aStream << " (will-change=";
    for (size_t i = 0; i < aItem->Frame()->StyleDisplay()->mWillChange.Length(); i++) {
      if (i > 0) {
        aStream << ",";
      }
      aStream << NS_LossyConvertUTF16toASCII(aItem->Frame()->StyleDisplay()->mWillChange[i]).get();
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
  uint32_t key = aItem->GetPerFrameKey();
  Layer* layer = mozilla::FrameLayerBuilder::GetDebugOldLayerFor(f, key);
  if (layer) {
    if (aDumpHtml) {
      aStream << nsPrintfCString(" <a href=\"#%p\">layer=%p</a>", layer, layer);
    } else {
      aStream << nsPrintfCString(" layer=0x%p", layer);
    }
  }
#ifdef MOZ_DUMP_PAINTING
  if (aItem->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
    nsCString str;
    (static_cast<nsDisplaySVGEffects*>(aItem))->PrintEffects(str);
    aStream << str.get();
  }
#endif
  aStream << "\n";

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
nsFrame::PrintDisplayItem(nsDisplayListBuilder* aBuilder,
                          nsDisplayItem* aItem,
                          std::stringstream& aStream,
                          bool aDumpSublist,
                          bool aDumpHtml)
{
  PrintDisplayItemTo(aBuilder, aItem, aStream, 0, aDumpSublist, aDumpHtml);
}

void
nsFrame::PrintDisplayList(nsDisplayListBuilder* aBuilder,
                          const nsDisplayList& aList,
                          std::stringstream& aStream,
                          bool aDumpHtml)
{
  PrintDisplayListTo(aBuilder, aList, aStream, 0, aDumpHtml);
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
