/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsDrawingSurfaceQT_h___
#define nsDrawingSurfaceQT_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceQT.h"

#include <qpainter.h>
#include <qpixmap.h>
#include <qimage.h>

class nsDrawingSurfaceQT : public nsIDrawingSurface,
                           public nsIDrawingSurfaceQT
{
public:
    nsDrawingSurfaceQT();
    virtual ~nsDrawingSurfaceQT();

    NS_DECL_ISUPPORTS

    //nsIDrawingSurface interface

    NS_IMETHOD Lock(PRInt32 aX, 
                    PRInt32 aY, 
                    PRUint32 aWidth, 
                    PRUint32 aHeight,
                    void **aBits, 
                    PRInt32 *aStride, 
                    PRInt32 *aWidthBytes,
                    PRUint32 aFlags);
    NS_IMETHOD Unlock(void);
    NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
    NS_IMETHOD IsOffscreen(PRBool *aOffScreen);
    NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
    NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

    //nsIDrawingSurfaceQT interface

    NS_IMETHOD Init(QPaintDevice * aPaintDevice, QPainter *aGC);
    NS_IMETHOD Init(QPainter *aGC, 
                    PRUint32 aWidth, 
                    PRUint32 aHeight, 
                    PRUint32 aFlags);

    // utility functions.
    PRInt32 GetDepth() { return mDepth; }
    QPainter * GetGC(void);
    QPaintDevice  * GetPaintDevice(void);

protected:
    NS_IMETHOD CommonInit();

private:
    /* general */
    QPaintDevice  *mPaintDevice;
    QPixmap       *mPixmap;
    QPainter	  *mGC;
    int		  mDepth;
    nsPixelFormat mPixFormat;
    PRUint32      mWidth;
    PRUint32      mHeight;
    PRUint32	  mFlags;
    PRBool        mIsOffscreen;

    /* for locks */
    QImage        mImage;
    PRInt32	  mLockX;
    PRInt32	  mLockY;
    PRUint32      mLockWidth;
    PRUint32      mLockHeight;
    PRUint32      mLockFlags;
    PRBool	  mLocked;
};

#endif
