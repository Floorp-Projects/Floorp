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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Karl Tomlinson <karlt+@karlt.net>
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

#include "gfxCachedTempSurface.h"
#include "gfxContext.h"

class CachedSurfaceExpirationTracker :
  public nsExpirationTracker<gfxCachedTempSurface,2> {

public:
  // With K = 2, this means that surfaces will be released when they are not
  // used for 1-2 seconds.
  enum { TIMEOUT_MS = 1000 };
  CachedSurfaceExpirationTracker()
    : nsExpirationTracker<gfxCachedTempSurface,2>(TIMEOUT_MS) {}

  ~CachedSurfaceExpirationTracker() {
    AgeAllGenerations();
  }

  virtual void NotifyExpired(gfxCachedTempSurface* aSurface) {
    RemoveObject(aSurface);
    aSurface->Expire();
  }

  static void MarkSurfaceUsed(gfxCachedTempSurface* aSurface)
  {
    if (aSurface->GetExpirationState()->IsTracked()) {
      sExpirationTracker->MarkUsed(aSurface);
      return;
    }

    if (!sExpirationTracker) {
      sExpirationTracker = new CachedSurfaceExpirationTracker();
    }
    sExpirationTracker->AddObject(aSurface);
  }

  static void RemoveSurface(gfxCachedTempSurface* aSurface)
  {
    if (!sExpirationTracker)
      return;

    if (aSurface->GetExpirationState()->IsTracked()) {
      sExpirationTracker->RemoveObject(aSurface);
    }
    if (sExpirationTracker->IsEmpty()) {
      delete sExpirationTracker;
      sExpirationTracker = nsnull;
    }
  }

private:
  static CachedSurfaceExpirationTracker* sExpirationTracker;
};

CachedSurfaceExpirationTracker*
CachedSurfaceExpirationTracker::sExpirationTracker = nsnull;

gfxCachedTempSurface::~gfxCachedTempSurface()
{
  CachedSurfaceExpirationTracker::RemoveSurface(this);
}

already_AddRefed<gfxContext>
gfxCachedTempSurface::Get(gfxASurface::gfxContentType aContentType,
                          const gfxRect& aRect,
                          gfxASurface* aSimilarTo)
{
  if (mSurface) {
    /* Verify the current buffer is valid for this purpose */
    if (mSize.width < aRect.width || mSize.height < aRect.height
        || mSurface->GetContentType() != aContentType) {
      mSurface = nsnull;
    } else {
      NS_ASSERTION(mType == aSimilarTo->GetType(),
                   "Unexpected surface type change");
    }
  }

  bool cleared = false;
  if (!mSurface) {
    mSize = gfxIntSize(PRInt32(ceil(aRect.width)), PRInt32(ceil(aRect.height)));
    mSurface = aSimilarTo->CreateSimilarSurface(aContentType, mSize);
    if (!mSurface)
      return nsnull;

    cleared = PR_TRUE;
#ifdef DEBUG
    mType = aSimilarTo->GetType();
#endif
  }
  mSurface->SetDeviceOffset(-aRect.TopLeft());

  nsRefPtr<gfxContext> ctx = new gfxContext(mSurface);
  ctx->Rectangle(aRect);
  ctx->Clip();
  if (!cleared && aContentType != gfxASurface::CONTENT_COLOR) {
    ctx->SetOperator(gfxContext::OPERATOR_CLEAR);
    ctx->Paint();
    ctx->SetOperator(gfxContext::OPERATOR_OVER);
  }

  CachedSurfaceExpirationTracker::MarkSurfaceUsed(this);

  return ctx.forget();
}
