/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsDrawingSurfaceQT_h___
#define nsDrawingSurfaceQT_h___

#include "nsIDrawingSurface.h"
#include "nsIDrawingSurfaceQT.h"

#include <qpainter.h>
#include <qpixmap.h>

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

    NS_IMETHOD Init(QPixmap * aPixmap, QPainter *aGC);
    NS_IMETHOD Init(QPainter *aGC, 
                    PRUint32 aWidth, 
                    PRUint32 aHeight, 
                    PRUint32 aFlags);

    // utility functions.
    QPainter * GetGC(void);
    QPixmap  * GetPixmap(void);

protected:
    NS_IMETHOD CommonInit();

private:
    /* general */
    QPixmap	      * mPixmap;
    QPainter	  * mGC;
    int		        mDepth;
    nsPixelFormat	mPixFormat;
    PRUint32        mWidth;
    PRUint32        mHeight;
    PRUint32	    mFlags;
    PRBool          mIsOffscreen;

    /* for locks */
    PRInt32	   mLockX;
    PRInt32	   mLockY;
    PRUint32   mLockWidth;
    PRUint32   mLockHeight;
    PRUint32   mLockFlags;
    PRBool	   mLocked;
    PRBool     mCleanup;
};

#endif
