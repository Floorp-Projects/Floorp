/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Nokia Corporation code.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#ifndef GFX_SHARED_IMAGESURFACE_H
#define GFX_SHARED_IMAGESURFACE_H

#ifdef HAVE_XSHM

#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPoint.h"

#include <stdlib.h>
#include <string.h>

#ifdef MOZ_WIDGET_QT
#include <QX11Info>
#endif

#ifdef MOZ_WIDGET_GTK2
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

#ifdef MOZ_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#endif

#include <sys/ipc.h>
#include <sys/shm.h>

class THEBES_API gfxSharedImageSurface : public gfxASurface {
public:
  gfxSharedImageSurface();
  ~gfxSharedImageSurface();

  // ImageSurface methods
  gfxImageFormat Format() const { return mFormat; }

  const gfxIntSize& GetSize() const { return mSize; }
  int Width() const { return mSize.width; }
  int Height() const { return mSize.height; }

  /**
   * Distance in bytes between the start of a line and the start of the
   * next line.
   */
  int Stride() const { return mStride; }

  /**
   * Returns a pointer for the image data. Users of this function can
   * write to it, but must not attempt to free the buffer.
   */
  unsigned char* Data() const { return mData; } // delete this data under us and die.

  /**
   * Returns the total size of the image data.
   */
  int GetDataSize() const { return mStride*mSize.height; }

  /**
   * Returns a pointer for the X-image structure.
   */
  XImage *image() const { return mXShmImage; }

  /**
   * Returns a gfxImageSurface as gfxASurface.
   */
  already_AddRefed<gfxASurface> getASurface(void);

  /**
   * Construct an shared image surface and XImage
   * @param aSize The size of the buffer
   * @param aFormat Format of the data
   * @see gfxImageFormat
   * @param aShmId Shared Memory ID of data created outside,
   *                if not specified, then class will allocate own shared memory
   * @display aDisplay display used for X-Image creation
   *                if not specified, then used system default
   *
   */
  bool Init(const gfxIntSize& aSize,
            gfxImageFormat aFormat = ImageFormatUnknown,
            int aShmId = -1,
            Display *aDisplay = NULL);

private:
  bool CreateInternal(int aShmid);
  long ComputeStride() const;
  inline bool ComputeDepth();
  inline bool ComputeFormat();

  unsigned int     mDepth;
  int              mShmId;

  gfxIntSize mSize;
  bool mOwnsData;

  unsigned char   *mData;
  gfxImageFormat   mFormat;
  long mStride;

  Display *mDisp;
  XShmSegmentInfo  mShmInfo;
  XImage          *mXShmImage;
};

#endif /* HAVE_XSHM */
#endif /* GFX_SHARED_IMAGESURFACE_H */
