/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard <blizzard@mozilla.org>.  
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __nsIThebesFontMetrics_h
#define __nsIThebesFontMetrics_h

#include "nsIFontMetrics.h"
#include "nsIRenderingContext.h"

class nsThebesRenderingContext;

class gfxFontGroup;

class nsIThebesFontMetrics : public nsIFontMetrics {
public:
    // Get the width for this string.  aWidth will be updated with the
    // width in points, not twips.  Callers must convert it if they
    // want it in another format.
    virtual nsresult GetWidth(const char* aString, PRUint32 aLength,
                              nscoord& aWidth, nsThebesRenderingContext *aContext) = 0;
    // aCachedOffset will be updated with a new offset.
    virtual nsresult GetWidth(const PRUnichar* aString, PRUint32 aLength,
                              nscoord& aWidth, PRInt32 *aFontID,
                              nsThebesRenderingContext *aContext) = 0;

    // Get the text dimensions for this string
    virtual nsresult GetTextDimensions(const PRUnichar* aString,
                                       PRUint32 aLength,
                                       nsTextDimensions& aDimensions, 
                                       PRInt32* aFontID) = 0;
    virtual nsresult GetTextDimensions(const char*         aString,
                                       PRInt32             aLength,
                                       PRInt32             aAvailWidth,
                                       PRInt32*            aBreaks,
                                       PRInt32             aNumBreaks,
                                       nsTextDimensions&   aDimensions,
                                       PRInt32&            aNumCharsFit,
                                       nsTextDimensions&   aLastWordDimensions,
                                       PRInt32*            aFontID) = 0;
    virtual nsresult GetTextDimensions(const PRUnichar*    aString,
                                       PRInt32             aLength,
                                       PRInt32             aAvailWidth,
                                       PRInt32*            aBreaks,
                                       PRInt32             aNumBreaks,
                                       nsTextDimensions&   aDimensions,
                                       PRInt32&            aNumCharsFit,
                                       nsTextDimensions&   aLastWordDimensions,
                                       PRInt32*            aFontID) = 0;

    // Draw a string using this font handle on the surface passed in.  
    virtual nsresult DrawString(const char *aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                const nscoord* aSpacing,
                                nsThebesRenderingContext *aContext) = 0;
    // aCachedOffset will be updated with a new offset.
    virtual nsresult DrawString(const PRUnichar* aString, PRUint32 aLength,
                                nscoord aX, nscoord aY,
                                PRInt32 aFontID,
                                const nscoord* aSpacing,
                                nsThebesRenderingContext *aContext) = 0;

#ifdef MOZ_MATHML
    // These two functions get the bounding metrics for this handle,
    // updating the aBoundingMetrics in Points.  This means that the
    // caller will have to update them to twips before passing it
    // back.
    virtual nsresult GetBoundingMetrics(const char *aString, PRUint32 aLength,
                                        nsThebesRenderingContext *aContext,
                                        nsBoundingMetrics &aBoundingMetrics) = 0;
    // aCachedOffset will be updated with a new offset.
    virtual nsresult GetBoundingMetrics(const PRUnichar *aString,
                                        PRUint32 aLength,
                                        nsThebesRenderingContext *aContext,
                                        nsBoundingMetrics &aBoundingMetrics) = 0;
#endif /* MOZ_MATHML */

    // Set the direction of the text rendering
    virtual nsresult SetRightToLeftText(PRBool aIsRTL) = 0;
    virtual PRBool GetRightToLeftText() = 0;
    virtual void SetTextRunRTL(PRBool aIsRTL) = 0;

    virtual PRInt32 GetMaxStringLength() = 0;

    virtual gfxFontGroup* GetThebesFontGroup() = 0;

    // Needs to be virtual and at this level so that its caller in gkgfx can
    // avoid linking against thebes.
    virtual gfxUserFontSet* GetUserFontSet() = 0;
};

#endif /* __nsIThebesFontMetrics_h */
