/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsScreenPh.h"

#include <Pt.h>
#include "nsPhGfxLog.h"
#include "nslog.h"

NS_IMPL_LOG(nsScreenPhLog)
#define PRINTF NS_LOG_PRINTF(nsScreenPhLog)
#define FLUSH  NS_LOG_FLUSH(nsScreenPhLog)

nsScreenPh :: nsScreenPh (  )
{
  nsresult    		res = NS_ERROR_FAILURE;
  PhSysInfo_t       SysInfo;
  PhRect_t          rect;
  char              *p = NULL;
  int               inp_grp = 0;
  PhRid_t           rid;
  PhRegion_t        region;
  
  NS_INIT_REFCNT();

  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsScreenPh::nsScreenPh Constructor called this=<%p>\n", this));


  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.

  /* Initialize the data members */
  /* Get the Screen Size and Depth*/
   p = getenv("PHIG");
   if (p)
   {
     inp_grp = atoi(p);

     PhQueryRids( 0, 0, inp_grp, Ph_INPUTGROUP_REGION, 0, 0, 0, &rid, 1 );
     PhRegionQuery( rid, &region, &rect, NULL, 0 );
     inp_grp = region.input_group;
     PhWindowQueryVisible( Ph_QUERY_INPUT_GROUP | Ph_QUERY_EXACT, 0, inp_grp, &rect );
     mWidth  = rect.lr.x - rect.ul.x + 1;
     mHeight = rect.lr.y - rect.ul.y + 1;  

     /* Get the System Info for the RID */
     if (!PhQuerySystemInfo(rid, NULL, &SysInfo))
     {
       PR_LOG(PhGfxLog, PR_LOG_ERROR,("nsScreenPh::nsScreenPh with aWidget: Error getting SystemInfo\n"));
     }
     else
     {
       /* Make sure the "color_bits" field is valid */
       if (SysInfo.gfx.valid_fields & Ph_GFX_COLOR_BITS)
       {
        mPixelDepth = SysInfo.gfx.color_bits;
       }
     }	
  }
  else
  {
    PRINTF("nsScreenPh::nsScreenPh The PHIG environment variable must be set, try setting it to 1\n");  
  }
}

nsScreenPh :: ~nsScreenPh()
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsScreenPh::~nsScreenPh Destructor called this=<%p>\n", this));

  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenPh, NS_GET_IID(nsIScreen))

#if 0
NS_IMETHODIMP 
nsScreenPh :: GetWidth(PRInt32 *aWidth)
{
  PR_LOG(PhGfxLog, PR_LOG_DEBUG, ("nsScreenPh::GetWidth Constructor called this=<%p>\n", this));

  *aWidth = mWidth;
  return NS_OK;

} // GetWidth


NS_IMETHODIMP 
nsScreenPh :: GetHeight(PRInt32 *aHeight)
{
  *aHeight = mHeight;
  return NS_OK;

} // GetHeight

#endif

NS_IMETHODIMP 
nsScreenPh :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  *aPixelDepth = mPixelDepth;
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenPh :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


#if 0
NS_IMETHODIMP 
nsScreenPh :: GetAvailWidth(PRInt32 *aAvailWidth)
{
  return GetWidth(aAvailWidth);

} // GetAvailWidth
#endif

#if 0
NS_IMETHODIMP 
nsScreenPh :: GetAvailHeight(PRInt32 *aAvailHeight)
{
  return GetHeight(aAvailHeight);

} // GetAvailHeight


NS_IMETHODIMP 
nsScreenPh :: GetAvailLeft(PRInt32 *aAvailLeft)
{
  *aAvailLeft = 0;
  return NS_OK;

} // GetAvailLeft



NS_IMETHODIMP 
nsScreenPh :: GetAvailTop(PRInt32 *aAvailTop)
{
  *aAvailTop = 0;
  return NS_OK;

} // GetAvailTop

#endif

NS_IMETHODIMP
nsScreenPh :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outTop = 0;
  *outLeft = 0;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenPh :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outTop = 0;
  *outLeft = 0;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetAvailRect
