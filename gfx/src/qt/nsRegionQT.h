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

#ifndef nsRegionQT_h___
#define nsRegionQT_h___

#include "nsIRegion.h"

#include <qregion.h>

class nsRegionQT : public nsIRegion
{
public:
    nsRegionQT();
    virtual ~nsRegionQT();

    NS_DECL_ISUPPORTS

    virtual nsresult Init();

    virtual void SetTo(const nsIRegion &aRegion);
    virtual void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    void SetTo(const nsRegionQT *aRegion);
    virtual void Intersect(const nsIRegion &aRegion);
    virtual void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    virtual void Union(const nsIRegion &aRegion);
    virtual void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    virtual void Subtract(const nsIRegion &aRegion);
    virtual void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    virtual PRBool IsEmpty(void);
    virtual PRBool IsEqual(const nsIRegion &aRegion);
    virtual void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);
    virtual void Offset(PRInt32 aXOffset, PRInt32 aYOffset);
    virtual PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    NS_IMETHOD GetRects(nsRegionRectSet **aRects);
    NS_IMETHOD FreeRects(nsRegionRectSet *aRects);
    NS_IMETHOD GetNativeRegion(void *&aRegion) const;
    NS_IMETHOD GetRegionComplexity(nsRegionComplexity &aComplexity) const;
    NS_IMETHOD GetNumRects(PRUint32* aRects) const;

private:
    virtual void SetRegionEmpty();

private:
    QRegion            mRegion;
    nsRegionComplexity mRegionType;
};

#endif  // nsRegionQT_h___ 
