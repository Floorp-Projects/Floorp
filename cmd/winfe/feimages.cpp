/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "stdafx.h"
#include "edt.h"
#include "prefapi.h"

//	Structures
//
// Yea I know pImageURL has no size!
//   Image strings are dynamically allocated starting there
typedef struct _FE_CopyImageData {
    int     iSize;              // size of all data, including variable length strings
    Bool    bIsMap;             // For server side maps
    int32   iWidth;             // Fixed size data correspond to fields in LO_ImageDataStruct
    int32   iHeight;            //   and EDT_ImageData
    int32   iHSpace;
    int32   iVSpace;
    int32   iBorder;
    int     iLowResOffset;      // Offsets into string_data. If 0, string is NULL (not used)
    int     iAltOffset;
    int     iAnchorOffset;      // HREF in image
    int     iExtraHTML_Offset;  // Extra HTML (stored in CImageElement)
#pragma warning( disable: 4200)
    char    pImageURL[];     // Append all variable-length strings starting here
#pragma warning( default: 4200)
} FE_CopyImageData;


// TODO: Should we include other image_attr fields? (e.g.: alignment, attrmask)

void FE_ShiftImage(MWContext *context, LO_ImageStruct *lo_image)	{
}

// free the mem using correct conditional call
inline void fe_PtrFreeBits(void XP_HUGE *pBits)
{
    if (pBits) {
#ifdef XP_WIN16
        _hfree(pBits);
#else
        free(pBits);
#endif                           
    }
}

// Dragged Image can only be dropped in Composer
#ifdef EDITOR
// Fill our structure with data about an image
//   for drag n drop and copying to clipboard
HGLOBAL WFE_CreateCopyImageData(MWContext *pMWContext, LO_ImageStruct *pImage)
{
	if( pMWContext == NULL ||
	    pImage == NULL ||
	    pImage->image_url == NULL) {
        return NULL;
    }

    char *pImageURL = NULL;
    char *pExtraHTML = EDT_GetExtraHTML_FromImage(pImage);

    // Lifted from GetImageHref() in POPUP.CPP:
	//	Check for that funky internal-external-reconnect jazz.
	char *pReconnect = "internal-external-reconnect:";
    // Don't use anything else that starts with this:
    // TODO: CHANGE THIS TO TEST FOR  pImage->bIsInternalImage AFTER lloyd IMPLEMENTS THAT
	char *pInternal = "internal-";

    if(strncmp((const char *)pImage->image_url, pReconnect, 28) == 0) {
    //	Need to take the URL off the history.
		//MWContext *pContext = GetContext()->GetContext();
		History_entry *pHistEnt = SHIST_GetCurrent(&pMWContext->hist);
        if ( pHistEnt && pHistEnt->address ){
		    pImageURL = pHistEnt->address;
        }
	}
    else if(strncmp((const char *)pImage->image_url, pInternal, 9) == 0) {
        return NULL;
	} else {
		pImageURL = (char *)pImage->image_url;
	}

    if(pImageURL && XP_STRLEN(pImageURL) > 0) {
        // We have an URL, the most important part,
        //   but save all the data in our drag structure
        int iLowResOffset = 0;
        int iAltOffset = 0;
        int iAnchorOffset = 0;
        int iExtraHTML_Offset = 0;

        // First calculate allocation size and offsets for the strings
        // Offset to pImageURL = 1 past end of struct size,
        //  so offset to next string is 1 past the '\0' of pImageURL
        int iOffset = sizeof( FE_CopyImageData ) + XP_STRLEN(pImageURL) + 2;

        if ( pImage->lowres_image_url ){
            iLowResOffset = iOffset;
            // Add size of string + 1 for '\0'
            iOffset += XP_STRLEN((char*)pImage->lowres_image_url) + 1;
        }
        if ( pImage->alt ){
            iAltOffset = iOffset;
            iOffset += XP_STRLEN((char*)pImage->alt) + 1;
        }
        if ( pImage->anchor_href ){
            if ( pImage->anchor_href->anchor ){
                iAnchorOffset = iOffset;
                iOffset += XP_STRLEN((char*)pImage->anchor_href->anchor) + 1;
            }
        }
        if ( pExtraHTML ){
            // Get Extra HTML if it exists
            iExtraHTML_Offset = iOffset;
            iOffset += XP_STRLEN(pExtraHTML) + 1;
        }
        // Total size = last offset calculated + extra '\0'
        int iDataSize = iOffset + 1;

        // Clipboard data should have GMEM_MOVEABLE,
        //  and it should be OK for drag and drop too?
        HGLOBAL hImageData = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE | GMEM_ZEROINIT, iDataSize);
        if(hImageData){
            FE_CopyImageData * pDragData = (FE_CopyImageData*)GlobalLock(hImageData);

            // Not used, but its good form to include size of all data
            pDragData->iSize  = iDataSize;
            pDragData->iWidth = pImage->width;
            pDragData->iHeight = pImage->height;
            pDragData->iHSpace = pImage->border_horiz_space;
            pDragData->iVSpace = pImage->border_vert_space;
            pDragData->iBorder = pImage->border_width;
            XP_STRCPY(pDragData->pImageURL, pImageURL);

            if ( pImage->image_attr && (pImage->image_attr->attrmask & LO_ATTR_ISMAP) ){
                pDragData->bIsMap = TRUE;
            }

            if (iLowResOffset){
                pDragData->iLowResOffset = iLowResOffset;
                XP_STRCPY( ((char*)pDragData)+iLowResOffset, 
                           (char*)pImage->lowres_image_url );
            }
            if (iAltOffset){
                pDragData->iAltOffset = iAltOffset; 
                XP_STRCPY( ((char*)pDragData)+iAltOffset,
                           (char*)pImage->alt );
            }
            if (iAnchorOffset){
                pDragData->iAnchorOffset = iAnchorOffset; 
                XP_STRCPY( ((char*)pDragData)+iAnchorOffset,
                           (char*)pImage->anchor_href->anchor);
            }
            if (iExtraHTML_Offset){
                pDragData->iExtraHTML_Offset = iExtraHTML_Offset;
                XP_STRCPY( ((char*)pDragData)+iExtraHTML_Offset, pExtraHTML );
                // Done with string - release it
                XP_FREE(pExtraHTML);
            }

            GlobalUnlock(hImageData);

            return hImageData;
            }
        }
    return NULL;
}

void WFE_DragDropImage(HGLOBAL h, MWContext *pMWContext)
{
    if( h ){
        FE_CopyImageData* pDragData = (FE_CopyImageData*) GlobalLock( h );
        if ( pDragData && XP_STRLEN(pDragData->pImageURL) > 0 ){
            EDT_ImageData* pEdtData = EDT_NewImageData();
        
            // Get the fixed-size data
            pEdtData->bIsMap = pDragData->bIsMap;
            pEdtData->iWidth = pDragData->iWidth;
            pEdtData->iHeight = pDragData->iHeight;
            pEdtData->iHSpace = pDragData->iHSpace;
            pEdtData->iVSpace = pDragData->iVSpace;
            pEdtData->iBorder = pDragData->iBorder;

            // Kludge: Sitemanager gives us D:/foo/bar 
            //  so convert to URL
            CString csURL;
            WFE_ConvertFile2Url(csURL, pDragData->pImageURL);

            pEdtData->pSrc = XP_STRDUP(csURL);

            // Get variable-allocated strings
            if ( pDragData->iLowResOffset ){
                pEdtData->pLowSrc =
                        XP_STRDUP(((char*)pDragData)+pDragData->iLowResOffset);
            }
            if ( pDragData->iAltOffset ){
                pEdtData->pAlt = XP_STRDUP(((char*)pDragData)+pDragData->iAltOffset);
            }
            if ( pDragData->iAnchorOffset ){
            }
            if ( pDragData->iExtraHTML_Offset ){
                pEdtData->pExtra =
                        XP_STRDUP(((char*)pDragData)+pDragData->iExtraHTML_Offset);
            // HARDTS: USEMAP data is now in pExtra, fix for bug 73283
            }
			int bKeepImages;
			PREF_GetBoolPref("editor.publish_keep_images",&bKeepImages);

            EDT_InsertImage(pMWContext, pEdtData,bKeepImages);
            EDT_FreeImageData(pEdtData);
        }
        GlobalUnlock( h );
    }
}
#endif /* EDITOR */    
