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

#ifndef GFX_CACHED_TEMP_SURFACE_H
#define GFX_CACHED_TEMP_SURFACE_H

#include "gfxASurface.h"
#include "nsExpirationTracker.h"

class gfxContext;

/**
 * This class can be used to cache double-buffering back surfaces.
 *
 * Large resource allocations may have an overhead that can be avoided by
 * caching.  Caching also alows the system to use history in deciding whether
 * to manage the surfaces in video or system memory.
 *
 * However, because we don't want to set aside megabytes of unused resources
 * unncessarily, these surfaces are released on a timer.
 */

class THEBES_API gfxCachedTempSurface {
public:
  /**
   * Returns a context for a surface that can be efficiently copied to
   * |aSimilarTo|.
   *
   * When |aContentType| has an alpha component, the surface will be cleared.
   * For opaque surfaces, the initial surface contents are undefined.
   * When |aContentType| differs in different invocations this is handled
   * appropriately, creating a new surface if necessary.
   * 
   * |aSimilarTo| should be of the same gfxSurfaceType in each invocation.
   * Because the cached surface may have been created during a previous
   * invocation, this will not be efficient if the new |aSimilarTo| has a
   * different format.
   */
  already_AddRefed<gfxContext> Get(gfxASurface::gfxContentType aContentType,
                                   const gfxRect& aRect,
                                   gfxASurface* aSimilarTo);

  void Expire() { mSurface = nsnull; }
  nsExpirationState* GetExpirationState() { return &mExpirationState; }
  ~gfxCachedTempSurface();

  bool IsSurface(gfxASurface* aSurface) { return mSurface == aSurface; }

private:
  nsRefPtr<gfxASurface> mSurface;
  gfxIntSize mSize;
  nsExpirationState mExpirationState;
#ifdef DEBUG
  gfxASurface::gfxSurfaceType mType;
#endif 
};

#endif /* GFX_CACHED_TEMP_SURFACE_H */
