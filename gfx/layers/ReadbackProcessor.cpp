/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "ReadbackProcessor.h"

namespace mozilla {
namespace layers {

void
ReadbackProcessor::BuildUpdates(ContainerLayer* aContainer)
{
  NS_ASSERTION(mAllUpdates.IsEmpty(), "Some updates not processed?");

  if (!aContainer->mMayHaveReadbackChild)
    return;

  aContainer->mMayHaveReadbackChild = PR_FALSE;
  // go backwards so the updates read from earlier layers are later in the
  // array.
  for (Layer* l = aContainer->GetLastChild(); l; l = l->GetPrevSibling()) {
    if (l->GetType() == Layer::TYPE_READBACK) {
      aContainer->mMayHaveReadbackChild = PR_TRUE;
      BuildUpdatesForLayer(static_cast<ReadbackLayer*>(l));
    }
  }
}

static Layer*
FindBackgroundLayer(ReadbackLayer* aLayer, nsIntPoint* aOffset)
{
  gfxMatrix transform;
  if (!aLayer->GetTransform().Is2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return nsnull;
  nsIntPoint transformOffset(PRInt32(transform.x0), PRInt32(transform.y0));

  for (Layer* l = aLayer->GetPrevSibling(); l; l = l->GetPrevSibling()) {
    gfxMatrix backgroundTransform;
    if (!l->GetTransform().Is2D(&backgroundTransform) ||
        backgroundTransform.HasNonIntegerTranslation())
      return nsnull;

    nsIntPoint backgroundOffset(PRInt32(backgroundTransform.x0), PRInt32(backgroundTransform.y0));
    nsIntRect rectInBackground(transformOffset - backgroundOffset, aLayer->GetSize());
    const nsIntRegion& visibleRegion = l->GetEffectiveVisibleRegion();
    if (!visibleRegion.Intersects(rectInBackground))
      continue;
    // Since l is present in the background, from here on we either choose l
    // or nothing.
    if (!visibleRegion.Contains(rectInBackground))
      return nsnull;

    if (l->GetEffectiveOpacity() != 1.0 ||
        !(l->GetContentFlags() & Layer::CONTENT_OPAQUE))
      return nsnull;

    // cliprects are post-transform
    const nsIntRect* clipRect = l->GetEffectiveClipRect();
    if (clipRect && !clipRect->Contains(nsIntRect(transformOffset, aLayer->GetSize())))
      return nsnull;

    Layer::LayerType type = l->GetType();
    if (type != Layer::TYPE_COLOR && type != Layer::TYPE_THEBES)
      return nsnull;

    *aOffset = backgroundOffset - transformOffset;
    return l;
  }

  return nsnull;
}

void
ReadbackProcessor::BuildUpdatesForLayer(ReadbackLayer* aLayer)
{
  if (!aLayer->mSink)
    return;

  nsIntPoint offset;
  Layer* newBackground = FindBackgroundLayer(aLayer, &offset);
  if (!newBackground) {
    aLayer->SetUnknown();
    return;
  }

  if (newBackground->GetType() == Layer::TYPE_COLOR) {
    ColorLayer* colorLayer = static_cast<ColorLayer*>(newBackground);
    if (aLayer->mBackgroundColor != colorLayer->GetColor()) {
      aLayer->mBackgroundLayer = nsnull;
      aLayer->mBackgroundColor = colorLayer->GetColor();
      NS_ASSERTION(aLayer->mBackgroundColor.a == 1.0,
                   "Color layer said it was opaque!");
      nsRefPtr<gfxContext> ctx =
          aLayer->mSink->BeginUpdate(aLayer->GetRect(),
                                     aLayer->AllocateSequenceNumber());
      if (ctx) {
        ctx->SetColor(aLayer->mBackgroundColor);
        nsIntSize size = aLayer->GetSize();
        ctx->Rectangle(gfxRect(0, 0, size.width, size.height));
        ctx->Fill();
        aLayer->mSink->EndUpdate(ctx, aLayer->GetRect());
      }
    }
  } else {
    NS_ASSERTION(newBackground->AsThebesLayer(), "Must be ThebesLayer");
    ThebesLayer* thebesLayer = static_cast<ThebesLayer*>(newBackground);
    // updateRect is relative to the ThebesLayer
    nsIntRect updateRect = aLayer->GetRect() - offset;
    if (thebesLayer != aLayer->mBackgroundLayer ||
        offset != aLayer->mBackgroundLayerOffset) {
      aLayer->mBackgroundLayer = thebesLayer;
      aLayer->mBackgroundLayerOffset = offset;
      aLayer->mBackgroundColor = gfxRGBA(0,0,0,0);
      thebesLayer->SetUsedForReadback(true);
    } else {
      nsIntRegion invalid;
      invalid.Sub(updateRect, thebesLayer->GetValidRegion());
      updateRect = invalid.GetBounds();
    }

    Update update = { aLayer, updateRect, aLayer->AllocateSequenceNumber() };
    mAllUpdates.AppendElement(update);
  }
}

void
ReadbackProcessor::GetThebesLayerUpdates(ThebesLayer* aLayer,
                                         nsTArray<Update>* aUpdates,
                                         nsIntRegion* aUpdateRegion)
{
  // All ThebesLayers used for readback are in mAllUpdates (some possibly
  // with an empty update rect).
  aLayer->SetUsedForReadback(false);
  if (aUpdateRegion) {
    aUpdateRegion->SetEmpty();
  }
  for (PRUint32 i = mAllUpdates.Length(); i > 0; --i) {
    const Update& update = mAllUpdates[i - 1];
    if (update.mLayer->mBackgroundLayer == aLayer) {
      aLayer->SetUsedForReadback(true);
      // Don't bother asking for updates if we have an empty update rect.
      if (!update.mUpdateRect.IsEmpty()) {
        aUpdates->AppendElement(update);
        if (aUpdateRegion) {
          aUpdateRegion->Or(*aUpdateRegion, update.mUpdateRect);
        }
      }
      mAllUpdates.RemoveElementAt(i - 1);
    }
  }
}

ReadbackProcessor::~ReadbackProcessor()
{
  for (PRUint32 i = mAllUpdates.Length(); i > 0; --i) {
    const Update& update = mAllUpdates[i - 1];
    // Unprocessed update. Notify the readback sink that this content is
    // unknown.
    update.mLayer->SetUnknown();
  }
}

}
}
