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
  virtual ~nsLayoutDebugger();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetShowFrameBorders(bool aEnable) MOZ_OVERRIDE;

  NS_IMETHOD GetShowFrameBorders(bool* aResult) MOZ_OVERRIDE;

  NS_IMETHOD SetShowEventTargetFrameBorder(bool aEnable) MOZ_OVERRIDE;

  NS_IMETHOD GetShowEventTargetFrameBorder(bool* aResult) MOZ_OVERRIDE;

  NS_IMETHOD GetContentSize(nsIDocument* aDocument,
                            int32_t* aSizeInBytesResult) MOZ_OVERRIDE;

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult) MOZ_OVERRIDE;

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult) MOZ_OVERRIDE;

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

#ifdef MOZ_DUMP_PAINTING
static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   FILE* aOutput, uint32_t aIndent, bool aDumpHtml);

static void
PrintDisplayItemTo(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
                   FILE* aOutput, uint32_t aIndent, bool aDumpSublist, bool aDumpHtml)
{
  nsCString str;
  if (!aDumpHtml) {
    for (uint32_t indent = 0; indent < aIndent; indent++) {
      str += "  ";
    }
  }
  nsIFrame* f = aItem->Frame();
  nsAutoString fName;
#ifdef DEBUG_FRAME_DUMP
  f->GetFrameName(fName);
#endif
  bool snap;
  nsRect rect = aItem->GetBounds(aBuilder, &snap);
  nscolor color;
  nsRect vis = aItem->GetVisibleRect();
  nsRect component = aItem->GetComponentAlphaBounds(aBuilder);
  nsDisplayList* list = aItem->GetChildren();
  const DisplayItemClip& clip = aItem->GetClip();
  nsRegion opaque;
  if (!list || list->DidComputeVisibility()) {
    opaque = aItem->GetOpaqueRegion(aBuilder, &snap);
  }
  if (aDumpHtml && aItem->Painted()) {
    nsCString string(aItem->Name());
    string.Append('-');
    string.AppendInt((uint64_t)aItem);
    str += nsPrintfCString("<a href=\"javascript:ViewImage('%s')\">", string.BeginReading());
  }
  str += nsPrintfCString("%s %p(%s) bounds(%d,%d,%d,%d) visible(%d,%d,%d,%d) componentAlpha(%d,%d,%d,%d) clip(%s) %s",
          aItem->Name(), (void*)f, NS_ConvertUTF16toUTF8(fName).get(),
          rect.x, rect.y, rect.width, rect.height,
          vis.x, vis.y, vis.width, vis.height,
          component.x, component.y, component.width, component.height,
          clip.ToString().get(),
          aItem->IsUniform(aBuilder, &color) ? " uniform" : "");
  nsRegionRectIterator iter(opaque);
  for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
    str += nsPrintfCString(" (opaque %d,%d,%d,%d)", r->x, r->y, r->width, r->height);
  }
  aItem->WriteDebugInfo(str);
  if (aDumpHtml && aItem->Painted()) {
    str += "</a>";
  }
  uint32_t key = aItem->GetPerFrameKey();
  Layer* layer = mozilla::FrameLayerBuilder::GetDebugOldLayerFor(f, key);
  if (layer) {
    if (aDumpHtml) {
      str += nsPrintfCString(" <a href=\"#%p\">layer=%p</a>", layer, layer);
    } else {
      str += nsPrintfCString(" layer=%p", layer);
    }
  }
  if (aItem->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
    (static_cast<nsDisplaySVGEffects*>(aItem))->PrintEffects(str);
  }
  fprintf_stderr(aOutput, "%s\n", str.get());
  if (aDumpSublist && list) {
    PrintDisplayListTo(aBuilder, *list, aOutput, aIndent+1, aDumpHtml);
  }
}

static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   FILE* aOutput, uint32_t aIndent, bool aDumpHtml)
{
  if (aDumpHtml) {
    fprintf_stderr(aOutput, "<ul>");
  }

  for (nsDisplayItem* i = aList.GetBottom(); i != nullptr; i = i->GetAbove()) {
    if (aDumpHtml) {
      fprintf_stderr(aOutput, "<li>");
    }
    PrintDisplayItemTo(aBuilder, i, aOutput, aIndent, true, aDumpHtml);
    if (aDumpHtml) {
      fprintf_stderr(aOutput, "</li>");
    }
  }

  if (aDumpHtml) {
    fprintf_stderr(aOutput, "</ul>");
  }
}

void
nsFrame::PrintDisplayItem(nsDisplayListBuilder* aBuilder,
                          nsDisplayItem* aItem,
                          FILE* aFile,
                          bool aDumpSublist,
                          bool aDumpHtml)
{
  PrintDisplayItemTo(aBuilder, aItem, aFile, 0, aDumpSublist, aDumpHtml);
}

void
nsFrame::PrintDisplayList(nsDisplayListBuilder* aBuilder,
                          const nsDisplayList& aList,
                          FILE* aFile,
                          bool aDumpHtml)
{
  PrintDisplayListTo(aBuilder, aList, aFile, 0, aDumpHtml);
}

static void
PrintDisplayListSetItem(nsDisplayListBuilder* aBuilder,
                        const char* aItemName,
                        const nsDisplayList& aList,
                        FILE* aFile,
                        bool aDumpHtml)
{
  if (aDumpHtml) {
    fprintf_stderr(aFile, "<li>");
  }
  fprintf_stderr(aFile, "%s", aItemName);
  PrintDisplayListTo(aBuilder, aList, aFile, 0, aDumpHtml);
  if (aDumpHtml) {
    fprintf_stderr(aFile, "</li>");
  }
}

void
nsFrame::PrintDisplayListSet(nsDisplayListBuilder* aBuilder,
                             const nsDisplayListSet& aSet,
                             FILE *aFile,
                             bool aDumpHtml)
{
  if (aDumpHtml) {
    fprintf_stderr(aFile, "<ul>");
  }
  PrintDisplayListSetItem(aBuilder, "[BorderBackground]", *(aSet.BorderBackground()), aFile, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[BlockBorderBackgrounds]", *(aSet.BlockBorderBackgrounds()), aFile, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Floats]", *(aSet.Floats()), aFile, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[PositionedDescendants]", *(aSet.PositionedDescendants()), aFile, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Outlines]", *(aSet.Outlines()), aFile, aDumpHtml);
  PrintDisplayListSetItem(aBuilder, "[Content]", *(aSet.Content()), aFile, aDumpHtml);
  if (aDumpHtml) {
    fprintf_stderr(aFile, "</ul>");
  }
}

#endif
