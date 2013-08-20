/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFXCOWSURFACEWRAPPER
#define GFXCOWSURFACEWRAPPER

#include "gfxImageSurface.h"
#include "nsISupportsImpl.h"
#include "nsAutoPtr.h"


/**
 * Provides an interface to implement a cross thread/process wrapper for a
 * gfxImageSurface that has copy-on-write semantics.
 *
 * Only the owner thread can write to the surface and acquire
 * read locks. Destroying a gfxReusableSurfaceWrapper releases
 * a read lock.
 *
 * OMTC Usage:
 * 1) Content creates a writable copy of this surface
 *    wrapper which will be optimized to the same wrapper if there
 *    are no readers.
 * 2) The surface is sent from content to the compositor once
 *    or potentially many times, each increasing a read lock.
 * 3) When the compositor receives the surface, it adopts the
 *    read lock.
 * 4) Once the compositor has processed the surface and uploaded
 *    the content, it then releases the read lock by dereferencing
 *    its wrapper.
 */
class gfxReusableSurfaceWrapper {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxReusableSurfaceWrapper)
public:
  virtual ~gfxReusableSurfaceWrapper() {}

  /**
   * Returns a read-only pointer to the image data.
   */
  virtual const unsigned char* GetReadOnlyData() const = 0;

  /**
   * Returns the image surface format.
   */
  virtual gfxASurface::gfxImageFormat Format() = 0;

  /**
   * Returns a writable copy of the image.
   * If necessary this will copy the wrapper. If there are no contention
   * the same wrapper will be returned. A ReadLock must be held when
   * calling this function, and calling it will give up this lock.
   */
  virtual gfxReusableSurfaceWrapper* GetWritable(gfxImageSurface** aSurface) = 0;

  /**
   * A read only lock count is recorded, any attempts to
   * call GetWritable() while this count is greater than one will
   * create a new surface/wrapper pair.
   *
   * When a surface's read count falls to zero, its memory will be
   * deallocated. It is the responsibility of the user to make sure
   * that all locks are matched with an equal number of unlocks.
   */
  virtual void ReadLock() = 0;
  virtual void ReadUnlock() = 0;

  /**
   * Types for each implementation of gfxReusableSurfaceWrapper.
   */
  enum Type {
    TYPE_SHARED_IMAGE,
    TYPE_IMAGE,

    TYPE_MAX
  };

  /**
   * Returns a unique ID for each implementation of gfxReusableSurfaceWrapper.
   */
  virtual Type GetType() = 0;

protected:
  NS_DECL_OWNINGTHREAD
};

#endif // GFXCOWSURFACEWRAPPER
