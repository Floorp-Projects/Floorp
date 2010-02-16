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

#include "gfxSharedImageSurface.h"
#include <stdio.h>

#ifdef HAVE_XSHM

gfxSharedImageSurface::gfxSharedImageSurface()
 : mDepth(0),
   mShmId(0),
   mOwnsData(true),
   mData(NULL),
   mStride(0),
   mXShmImage(NULL)
{
    memset(&mShmInfo, 0, sizeof(XShmSegmentInfo));
}

gfxSharedImageSurface::~gfxSharedImageSurface()
{
    // Finish all pending XServer operations
    if (mDisp) {
        XSync(mDisp, False);
        XShmDetach(mDisp, &mShmInfo);
    }

    if (mData)
        shmdt(mData);

    if (mXShmImage)
        XDestroyImage(mXShmImage);
}

already_AddRefed<gfxASurface>
gfxSharedImageSurface::getASurface(void)
{
    NS_ENSURE_TRUE(mData, NULL);

    gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatRGB24;
    if (mDepth == 32)
        imageFormat = gfxASurface::ImageFormatARGB32;

    gfxASurface* result = new gfxImageSurface(mData, mSize, mStride, imageFormat);
    NS_IF_ADDREF(result);
    return result;
}

#ifdef MOZ_WIDGET_QT
static unsigned int
getSystemDepth()
{
    return QX11Info::appDepth();
}

static Display*
getSystemDisplay()
{
    return QX11Info::display();
}
#endif
#ifdef MOZ_WIDGET_GTK2
static unsigned int
getSystemDepth()
{
    return gdk_visual_get_system()->depth;
}

static Display*
getSystemDisplay()
{
    return GDK_DISPLAY();
}
#endif



bool
gfxSharedImageSurface::Init(const gfxIntSize& aSize,
                            gfxImageFormat aFormat,
                            int aShmId,
                            Display *aDisplay)
{
    mSize = aSize;

    if (aFormat != ImageFormatUnknown) {
        mFormat = aFormat;
        if (!ComputeDepth())
            return false;
    } else {
        mDepth = getSystemDepth();
        if (!ComputeFormat())
            NS_WARNING("Will work with system depth");
    }

    mDisp = aDisplay ? aDisplay : getSystemDisplay();
    if (!mDisp)
        return false;

    return CreateInternal(aShmId);
}

bool
gfxSharedImageSurface::CreateInternal(int aShmid)
{
    memset(&mShmInfo, 0, sizeof(mShmInfo));

    mStride = ComputeStride();

    if (aShmid != -1) {
        mShmId = aShmid;
        mOwnsData = false;
    } else
        mShmId = shmget(IPC_PRIVATE, GetDataSize(), IPC_CREAT | 0777);

    if (mShmId == -1)
        return false;

    void *data = shmat(mShmId, 0, 0);
    shmctl(mShmId, IPC_RMID, 0);
    if (data == (void *)-1)
        return false;
    mData = (unsigned char *)data;

    mXShmImage = XShmCreateImage(mDisp, NULL,
                                 mDepth, ZPixmap,
                                 NULL, &mShmInfo,
                                 mSize.width,
                                 mSize.height);
    if (!mXShmImage)
        return false;
    mShmInfo.shmid = mShmId;

    if (mStride != mXShmImage->bytes_per_line) {
        NS_WARNING("XStride and gfxSharedImageSurface calculated stride does not match");
        return false;
    }

    mShmInfo.shmaddr = mXShmImage->data = (char*)data;
    mShmInfo.readOnly = False;
    if (!XShmAttach(mDisp, &mShmInfo))
        return false;

    return true;
}

bool
gfxSharedImageSurface::ComputeFormat()
{
    if (mDepth == 32)
        mFormat = ImageFormatARGB32;
    if (mDepth == 24)
        mFormat = ImageFormatRGB24;
    else {
        NS_WARNING("Unknown depth specified to gfxSharedImageSurface!");
        mFormat = ImageFormatUnknown;
        return false;
    }

    return true;
}

bool
gfxSharedImageSurface::ComputeDepth()
{
    mDepth = 0;
    if (mFormat == ImageFormatARGB32)
        mDepth = 32;
    else if (mFormat == ImageFormatRGB24)
        mDepth = 24;
    else {
        NS_WARNING("Unknown format specified to gfxSharedImageSurface!");
        return false;
    }

    return true;
}

long
gfxSharedImageSurface::ComputeStride() const
{
    long stride;

    if (mDepth == 16)
        stride = mSize.width * 2;
    else if (mFormat == ImageFormatARGB32)
        stride = mSize.width * 4;
    else if (mFormat == ImageFormatRGB24)
        stride = mSize.width * 4;
    else if (mFormat == ImageFormatA8)
        stride = mSize.width;
    else if (mFormat == ImageFormatA1) {
        stride = (mSize.width + 7) / 8;
    } else {
        NS_WARNING("Unknown format specified to gfxImageSurface!");
        stride = mSize.width * 4;
    }

    stride = ((stride + 3) / 4) * 4;

    return stride;
}

#endif /* HAVE_XSHM */
