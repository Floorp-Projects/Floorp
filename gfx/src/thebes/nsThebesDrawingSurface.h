/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is thebes gfx
 *
 * The Initial Developer of the Original Code is 
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _NSTHEBESDRAWINGSURFACE_H_
#define _NSTHEBESDRAWINGSURFACE_H_

#include "nsCOMPtr.h"

#include "nsSize.h"
#include "nsRect.h"
#include "nsIDrawingSurface.h"
#include "nsIWidget.h"

#include "gfxASurface.h"

class nsThebesDeviceContext;

class nsThebesDrawingSurface : public nsIDrawingSurface
{
public:
    nsThebesDrawingSurface ();
    virtual ~nsThebesDrawingSurface ();

    // create a image surface if aFastAccess == TRUE, otherwise create
    // a fast server pixmap
    nsresult Init (nsThebesDeviceContext *aDC, PRUint32 aWidth, PRUint32 aHeight, PRBool aFastAccess);

    // create a fast drawing surface for a native widget
    nsresult Init (nsThebesDeviceContext *aDC, nsIWidget *aWidget);

    // create a fast drawing surface for a native widget
    nsresult Init (nsThebesDeviceContext *aDC, nsNativeWidget aWidget);

    // create a drawing surface for a native pixmap
    nsresult Init (nsThebesDeviceContext* aDC, void* aNativePixmapBlack,
                   void* aNativePixmapWhite,
                   const nsIntSize& aSize);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDrawingSurface interface

    NS_IMETHOD Lock(PRInt32 aX, PRInt32 aY, PRUint32 aWidth, PRUint32 aHeight,
                    void **aBits, PRInt32 *aStride, PRInt32 *aWidthBytes,
                    PRUint32 aFlags);
    NS_IMETHOD Unlock(void);
    NS_IMETHOD GetDimensions(PRUint32 *aWidth, PRUint32 *aHeight);
    NS_IMETHOD IsOffscreen(PRBool *aOffScreen);
    NS_IMETHOD IsPixelAddressable(PRBool *aAddressable);
    NS_IMETHOD GetPixelFormat(nsPixelFormat *aFormat);

    /* utility functions */
    gfxASurface *GetThebesSurface(void) { return mSurface; }
    PRInt32 GetDepth() { /* XXX */ return 24; }
    nsNativeWidget GetNativeWidget(void) { return mNativeWidget; }

    nsresult PushFilter(const nsIntRect& aRect, PRBool aAreaIsOpaque, float aOpacity);
    void PopFilter();

    static PRBool UseGlitz();
    static PRInt32 mGlitzMode;
private:
    nsRefPtr<gfxASurface> mSurface;
    nsThebesDeviceContext *mDC;
    nsNativeWidget mNativeWidget;
#ifdef XP_WIN
    nsIWidget *mWidget;
#endif

    PRUint32 mWidth, mHeight;
};

#endif /* _NSTHEBESDRAWINGSURFACE_H_ */
