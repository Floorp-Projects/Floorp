/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * implementation of interface that allows layout-debug extension access
 * to some internals of layout
 */

#include "nsILayoutDebugger.h"
#include "nsFrame.h"
#include "nsDisplayList.h"
#include "FrameLayerBuilder.h"

#include <stdio.h>

using namespace mozilla::layers;

#ifdef NS_DEBUG
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
                            PRInt32* aSizeInBytesResult);

  NS_IMETHOD GetFrameSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult);

  NS_IMETHOD GetStyleSize(nsIPresShell* aPresentation,
                          PRInt32* aSizeInBytesResult);

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
                                 PRInt32* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLayoutDebugger::GetFrameSize(nsIPresShell* aPresentation,
                               PRInt32* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLayoutDebugger::GetStyleSize(nsIPresShell* aPresentation,
                               PRInt32* aSizeInBytesResult)
{
  *aSizeInBytesResult = 0;
  return NS_ERROR_FAILURE;
}
#endif

#ifdef MOZ_DUMP_PAINTING
static void
PrintDisplayListTo(nsDisplayListBuilder* aBuilder, const nsDisplayList& aList,
                   PRInt32 aIndent, FILE* aOutput)
{
  for (nsDisplayItem* i = aList.GetBottom(); i != nsnull; i = i->GetAbove()) {
#ifdef DEBUG
    if (aList.DidComputeVisibility() && i->GetVisibleRect().IsEmpty())
      continue;
#endif
    for (PRInt32 j = 0; j < aIndent; ++j) {
      fputc(' ', aOutput);
    }
    nsIFrame* f = i->GetUnderlyingFrame();
    nsAutoString fName;
#ifdef DEBUG
    if (f) {
      f->GetFrameName(fName);
    }
#endif
    nsRect rect = i->GetBounds(aBuilder);
    switch (i->GetType()) {
      case nsDisplayItem::TYPE_CLIP:
      case nsDisplayItem::TYPE_CLIP_ROUNDED_RECT: {
        nsDisplayClip* c = static_cast<nsDisplayClip*>(i);
        rect = c->GetClipRect();
        break;
      }
      default:
        break;
    }
    nscolor color;
    nsRect vis = i->GetVisibleRect();
    nsDisplayList* list = i->GetList();
    nsRegion opaque;
    if (i->GetType() == nsDisplayItem::TYPE_TRANSFORM) {
        nsDisplayTransform* t = static_cast<nsDisplayTransform*>(i);
        list = t->GetStoredList()->GetList();
    }
#ifdef DEBUG
    if (!list || list->DidComputeVisibility()) {
      opaque = i->GetOpaqueRegion(aBuilder);
    }
#endif
    fprintf(aOutput, "%s %p(%s) (%d,%d,%d,%d)(%d,%d,%d,%d)%s%s",
            i->Name(), (void*)f, NS_ConvertUTF16toUTF8(fName).get(),
            rect.x, rect.y, rect.width, rect.height,
            vis.x, vis.y, vis.width, vis.height,
            opaque.IsEmpty() ? "" : " opaque",
            i->IsUniform(aBuilder, &color) ? " uniform" : "");
    if (f) {
      PRUint32 key = i->GetPerFrameKey();
      Layer* layer = aBuilder->LayerBuilder()->GetOldLayerFor(f, key);
      if (layer) {
        fprintf(aOutput, " layer=%p", layer);
      }
    }
    if (i->GetType() == nsDisplayItem::TYPE_SVG_EFFECTS) {
      (static_cast<nsDisplaySVGEffects*>(i))->PrintEffects(aOutput);
    }
    fputc('\n', aOutput);
    if (list) {
      PrintDisplayListTo(aBuilder, *list, aIndent + 4, aOutput);
    }
  }
}

void
nsFrame::PrintDisplayList(nsDisplayListBuilder* aBuilder,
                          const nsDisplayList& aList)
{
  PrintDisplayListTo(aBuilder, aList, 0, stdout);
}

#endif
