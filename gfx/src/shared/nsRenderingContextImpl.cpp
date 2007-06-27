/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCOMPtr.h"
#include "nsRenderingContextImpl.h"
#include "nsIDeviceContext.h"
#include "nsIImage.h"
#include "nsIRegion.h"
#include "nsIFontMetrics.h"
#include <stdlib.h>


/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: nsRenderingContextImpl()
{
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
nsRenderingContextImpl :: ~nsRenderingContextImpl()
{


}

NS_IMETHODIMP
nsRenderingContextImpl::GetRightToLeftText(PRBool* aIsRTL)
{
  *aIsRTL = PR_FALSE;
  return NS_OK;
}

#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000
static PRInt32 GetMaxChunkLength(nsRenderingContextImpl* aContext)
{
  PRInt32 len = aContext->GetMaxStringLength();
  return PR_MIN(len, MAX_GFX_TEXT_BUF_SIZE);
}

static PRInt32 FindSafeLength(nsRenderingContextImpl* aContext,
                              const PRUnichar *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
  if (aLength <= aMaxChunkLength)
    return aLength;
  
  PRUint8 buffer[MAX_GFX_TEXT_BUF_SIZE + 1];
  // Fill in the cluster hint information, if it's available.
  PRUint32 clusterHint;
  aContext->GetHints(clusterHint);
  clusterHint &= NS_RENDERING_HINT_TEXT_CLUSTERS;

  PRInt32 len = aMaxChunkLength;

  if (clusterHint) {
    nsresult rv =
      aContext->GetClusterInfo(aString, aMaxChunkLength + 1, buffer);
    if (NS_FAILED(rv))
      return len;
  }

  // Ensure that we don't break inside a cluster or inside a surrogate pair
  while (len > 0 &&
         (NS_IS_LOW_SURROGATE(aString[len]) || (clusterHint && !buffer[len]))) {
    len--;
  }
  if (len == 0) {
    // We don't want our caller to go into an infinite loop, so don't return
    // zero. It's hard to imagine how we could actually get here unless there
    // are languages that allow clusters of arbitrary size. If there are and
    // someone feeds us a 500+ character cluster, too bad.
    return aMaxChunkLength;
  }
  return len;
}

static PRInt32 FindSafeLength(nsRenderingContextImpl* aContext,
                              const char *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
  // Since it's ASCII, we don't need to worry about clusters or RTL
  return PR_MIN(aLength, aMaxChunkLength);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const nsString& aString, nscoord &aWidth,
                                 PRInt32 *aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                   PRInt32 aFontID, const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const char* aString, PRUint32 aLength,
                                 nscoord& aWidth)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  aWidth = 0;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width;
    nsresult rv = GetWidthInternal(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetWidth(const PRUnichar *aString, PRUint32 aLength,
                                 nscoord &aWidth, PRInt32 *aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  aWidth = 0;
  
  if (aFontID) {
    *aFontID = 0;
  }
  
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width;
    nsresult rv = GetWidthInternal(aString, len, width);
    if (NS_FAILED(rv))
      return rv;
    aWidth += width;
    aLength -= len;
    aString += len;
  }
  return NS_OK;
}  

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const char* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetTextDimensionsInternal(aString, aLength, aDimensions);
 
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsTextDimensions dimensions;
    nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                          nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetTextDimensionsInternal(aString, aLength, aDimensions);
    
  if (aFontID) {
    *aFontID = nsnull;
  }
 
  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsTextDimensions dimensions;
    nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsTextDimensions, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned.
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(dimensions);
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }
  return NS_OK;  
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const char*       aString,
                                          PRInt32           aLength,
                                          PRInt32           aAvailWidth,
                                          PRInt32*          aBreaks,
                                          PRInt32           aNumBreaks,
                                          nsTextDimensions& aDimensions,
                                          PRInt32&          aNumCharsFit,
                                          nsTextDimensions& aLastWordDimensions,
                                          PRInt32*          aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= PRInt32(maxChunkLength))
    return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                     aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }
  
  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    nsTextDimensions dimensions;
    
    NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
    NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");
   
     // Call safe method

    nsresult rv =
      GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                        dimensions);
    if (NS_FAILED(rv))
      return rv;
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetTextDimensions(const PRUnichar*  aString,
                                          PRInt32           aLength,
                                          PRInt32           aAvailWidth,
                                          PRInt32*          aBreaks,
                                          PRInt32           aNumBreaks,
                                          nsTextDimensions& aDimensions,
                                          PRInt32&          aNumCharsFit,
                                          nsTextDimensions& aLastWordDimensions,
                                          PRInt32*          aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= PRInt32(maxChunkLength))
    return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                     aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }

  // Do a naive implementation based on 3-arg GetTextDimensions
  PRInt32 x = 0;
  PRInt32 wordCount;
  for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
    PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
    
    NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
    NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");
    
    nsTextDimensions dimensions;
    // Call safe method
    nsresult rv =
      GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                        dimensions);
    if (NS_FAILED(rv))
      return rv;
    x += dimensions.width;
    // The first word always "fits"
    if (x > aAvailWidth && wordCount > 0)
      break;
    // aDimensions ascent/descent should exclude the last word (unless there
    // is only one word) so we let it run one word behind
    if (wordCount == 0) {
      aDimensions = dimensions;
    } else {
      aDimensions.Combine(aLastWordDimensions);
    }
    aNumCharsFit = aBreaks[wordCount];
    aLastWordDimensions = dimensions;
  }
  // aDimensions width should include all the text
  aDimensions.width = x;
  return NS_OK;
}
#endif

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsRenderingContextImpl::GetBoundingMetrics(const char*        aString,
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics);

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics;
    nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }  
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::GetBoundingMetrics(const PRUnichar*   aString,
                                           PRUint32           aLength,
                                           nsBoundingMetrics& aBoundingMetrics,
                                           PRInt32*           aFontID)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength)
    return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics, aFontID);

  if (aFontID) {
    *aFontID = 0;
  }

  PRBool firstIteration = PR_TRUE;
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsBoundingMetrics metrics;
    nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
    if (NS_FAILED(rv))
      return rv;
    if (firstIteration) {
      // Instead of combining with a Clear()ed nsBoundingMetrics, we assign
      // directly in the first iteration. This ensures that negative ascent/
      // descent can be returned and the left bearing is properly initialized.
      aBoundingMetrics = metrics;
    } else {
      aBoundingMetrics += metrics;
    }
    aLength -= len;
    aString += len;
    firstIteration = PR_FALSE;
  }  
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nsresult rv = DrawStringInternal(aString, len, aX, aY);
    if (NS_FAILED(rv))
      return rv;
    aLength -= len;

    if (aLength > 0) {
      nscoord width;
      rv = GetWidthInternal(aString, len, width);
      if (NS_FAILED(rv))
        return rv;
      aX += width;
      aString += len;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextImpl::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
  PRUint32 maxChunkLength = GetMaxChunkLength(this);
  if (aLength <= maxChunkLength) {
    return DrawStringInternal(aString, aLength, aX, aY, aFontID, aSpacing);
  }

  PRBool isRTL = PR_FALSE;
  GetRightToLeftText(&isRTL);

  if (isRTL) {
    nscoord totalWidth = 0;
    if (aSpacing) {
      for (PRUint32 i = 0; i < aLength; ++i) {
        totalWidth += aSpacing[i];
      }
    } else {
      nsresult rv = GetWidth(aString, aLength, totalWidth);
      if (NS_FAILED(rv))
        return rv;
    }
    aX += totalWidth;
  }
  
  while (aLength > 0) {
    PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
    nscoord width = 0;
    if (aSpacing) {
      for (PRInt32 i = 0; i < len; ++i) {
        width += aSpacing[i];
      }
    } else {
      nsresult rv = GetWidthInternal(aString, len, width);
      if (NS_FAILED(rv))
        return rv;
    }

    if (isRTL) {
      aX -= width;
    }
    nsresult rv = DrawStringInternal(aString, len, aX, aY, aFontID, aSpacing);
    if (NS_FAILED(rv))
      return rv;
    aLength -= len;
    if (!isRTL) {
      aX += width;
    }
    aString += len;
    if (aSpacing) {
      aSpacing += len;
    }
  }
  return NS_OK;
}
