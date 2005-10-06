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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
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

/*
 * *********************************************************
 * This file is under construction.  Ignore it for a while.
 * *********************************************************
 */

class NS_EXPORT gfxRegion {
public:
    gfxRegion(const gfxRegion& r);
    gfxRegion(const gfxRect& r);

    gfxRegion& operator=(const gfxRegion& r);
    gfxRegion& operator=(const gfxRect& r);

    int operator==(const gfxRegion& r) const;
    int operator!=(const gfxRegion& r) const;

    gfxPoint& operator+=(const gfxRegion& r); // Union
    gfxPoint& operator+=(const gfxRect& r); // Union
    gfxPoint& operator-=(const gfxRegion& r); // Subtract
    gfxPoint& operator-=(const gfxRect& r); // Subtract



    void SetTo(const nsIRegion &aRegion);
    void SetTo(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    void Intersect(const nsIRegion &aRegion);
    void Intersect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    void Union(const nsIRegion &aRegion);
    void Union(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    void Subtract(const nsIRegion &aRegion);
    void Subtract(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    PRBool IsEmpty(void);
    PRBool IsEqual(const nsIRegion &aRegion);
    void GetBoundingBox(PRInt32 *aX, PRInt32 *aY, PRInt32 *aWidth, PRInt32 *aHeight);
    void Offset(PRInt32 aXOffset, PRInt32 aYOffset);
    PRBool ContainsRect(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight);
    void GetRects(nsRegionRectSet **aRects);
    void FreeRects(nsRegionRectSet *aRects);
    void GetNativeRegion(void *&aRegion) const;
    void GetRegionComplexity(nsRegionComplexity &aComplexity) const;
    void GetNumRects(PRUint32 *aRects) const;
};
