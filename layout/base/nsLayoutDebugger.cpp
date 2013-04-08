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

#include <stdio.h>

using namespace mozilla;
using namespace mozilla::layers;

#ifdef DEBUG
class nsLayoutDebugger : public nsILayoutDebugger {
public:
  nsLayoutDebugger();
  virtual ~nsLayoutDebugger();

  NS_DECL_ISUPPORTS

  NS_IMETHOD SetShowFrameBorders(bool aEnable);

  NS_IMETHOD GetShowFrameBorders(bool* aResult);

  NS_IMETHOD SetShowEventTargetFrameBorder(bool aEnable);

  NS_IMETHOD GetShowEventTargetFrameBorder(bool* aResult);

  NS_IMETHOD GetContentSize(nsIDocument* aDocument,
                            int32_t* aSizeInBytesResult);

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult);

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          int32_t* aSizeInBytesResult);

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

NS_IMPL_ISUPPORTS1(nsLayoutDebugger, nsILayoutDebugger)

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
static int sPrintDisplayListIndent = 0;

static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   FILE* aOutput, bool aDumpHtml)
{
  if (aDumpHtml) {
    fprintf(aOutput, "<ul>");
  }

  for (nsDisplayItem* i = aList.GetBottom(); i != nullptr; i = i->GetAbove()) {
    if (aDumpHtml) {
      fprintf(aOutput, "<li>");
    } else {
      sPrintDisplayListIndent ++;
      for (int indent = 0; indent < sPrintDisplayListIndent; indent++) {
        fprintf(aOutput, "  ");
      }
    }
    nsIFrame* f = i->GetUnderlyingFrame();
    nsAutoString fName;
#ifdef DEBUG
    if (f) {
      f->GetFrameName(fName);
    }
#endif
    bool snap;
    nsRect rect = i->GetBounds(aBuilder, &snap);
    nscolor color;
    nsRect vis = i->GetVisibleRect();
    nsRect component = i->GetComponentAlphaBounds(aBuilder);
    nsDisplayList* list = i->GetChildren();
    const DisplayItemClip& clip = i->GetClip();
    nsRegion opaque;
#ifdef DEBUG
    if (!list || list->DidComputeVisibility()) {
      opaque = i->GetOpaqueRegion(aBuilder, &snap);
    }
#endif
    if (aDumpHtml && i->Painted()) {
      nsCString string(i->Name());
      string.Append("-");
      string.AppendInt((uint64_t)i);
      fprintf(aOutput, "<a href=\"javascript:ViewImage('%s')\">", string.BeginReading());
    }
    fprintf(aOutput, "%s %p(%s) bounds(%d,%d,%d,%d) visible(%d,%d,%d,%d) componentAlpha(%d,%d,%d,%d) clip(%s) %s",
            i->Name(), (void*)f, NS_ConvertUTF16toUTF8(fName).get(),
            rect.x, rect.y, rect.width, rect.height,
            vis.x, vis.y, vis.width, vis.height,
            component.x, component.y, component.width, component.height,
            clip.ToString().get(),
            i->IsUniform(aBuilder, &color) ? " uniform" : "");
    nsRegionRectIterator iter(opaque);
    for (const nsRect* r = iter.Next(); r; r = iter.Next()) {
      fprintf(aOutput, " (opaque %d,%d,%d,%d)", r->x, r->y, r->width, r->height);
    }
    i->WriteDebugInfo(aOutput);
    if (aDumpHtml && i->Painted()) {
      fprintf(aOutput, "</a>");
    }
    if (f) {
      uint32_t key = i->GetPerFrameKey();
      Layer* layer = mozilla::FrameLayerBuilder::GetDebugOldLayerFor(f, key);
      if (layer) {
        if (aDumpHtml) {
          fprintf(aOutput, " <a href=\"#%p\">layer=%p</a>", layer, layer);
        } else {
          fprintf(aOutput, " layer=%p", layer);
        }
      }
    }
    if (i->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
      (static_cast<nsDisplaySVGEffects*>(i))->PrintEffects(aOutput);
    }
    fputc('\n', aOutput);
    if (list) {
      PrintDisplayListTo(aBuilder, *list, aOutput, aDumpHtml);
    }
    if (aDumpHtml) {
      fprintf(aOutput, "</li>");
    } else {
      sPrintDisplayListIndent --;
    }
  }

  if (aDumpHtml) {
    fprintf(aOutput, "</ul>");
  }
}

void
nsFrame::PrintDisplayList(nsDisplayListBuilder* aBuilder,
                          const nsDisplayList& aList,
                          FILE* aFile,
                          bool aDumpHtml)
{
  PrintDisplayListTo(aBuilder, aList, aFile, aDumpHtml);
}

#endif
