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

#include "feembed.h"

#include "cntritem.h"
#include "ngdwtrst.h"
#include "cxdc.h"
#include "npapi.h"
#include "np.h"
#include "presentm.h"
#include "helper.h"
#include "il_icons.h"
#include "extgen.h"
#include "libevent.h"

extern "C" int MK_DISK_FULL;   // defined in allxpstr.c

extern char *FE_FindFileExt(char * path);

extern "C"	{
 
BOOL wfe_IsTypePlugin(NPEmbeddedApp* pEmbeddedApp) 
{
    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
	return (pEmbeddedApp->type == NP_Plugin) ? TRUE : FALSE;
}

CNetscapeCntrItem *wfe_GetCntrPtr(void* pDataObj) // where pDataObj is an URL struct
{
    NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)((URL_Struct*)pDataObj)->fe_data;
    return (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
}

NPError FE_PluginGetValue(MWContext *pContext, NPEmbeddedApp *pApp, 
                          NPNVariable variable, void *pRetVal)
{
    NPError ret = NPERR_NO_ERROR;
    
    switch (variable) {
        case NPNVnetscapeWindow:
        {
            if (pContext->type != MWContextPrint)
                *(HWND *)pRetVal = PANECX(pContext)->GetPane();
            else
                ret = NPERR_INVALID_PARAM;
        }
         break;
        default:
            *(void **)pRetVal = NULL;
            break;
    }

    return ret;
}

// wrapper for plugin-related FE entry point invented after EmbedUrlExit()
void FE_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx)
{
    EmbedUrlExit(urls, status, cx);
}

void EmbedUrlExit(URL_Struct *pUrl, int iStatus, MWContext *pContext)
{
    //  The embedded item is finished downloading, and possibly has an error and stuff.
    NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pUrl->fe_data;

	if (!pEmbeddedApp) {
		NET_FreeURLStruct(pUrl);
		return;
	}

    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;

    // if an EMBED tag's SRC attribute is a LOCAL file which does not exist,
    // pItem is NULL.  Bandaid against GPF for 2.0, but later we must fix the
    // FE_GetEmbedSize() and NP_EmbedCreate() combo that cause this problem.
    if(pItem != NULL) {
	    if(wfe_IsTypePlugin(pEmbeddedApp))
	    {
            pItem->m_bLoading = FALSE;
			NET_FreeURLStruct(pUrl);
        	return;
	    }
#ifdef MOCHA
	    {
		    /* only wait on applets if onload flag */
		    lo_TopState *top_state = lo_FetchTopState(XP_DOCID(pContext));
		    if (top_state != NULL && top_state->mocha_loading_embeds_count)
		    {
			    top_state->mocha_loading_embeds_count--;
			    ET_SendLoadEvent(pContext, EVENT_XFER_DONE, NULL, NULL, 
                                 LO_DOCUMENT_LAYER_ID, FALSE);
		    }
	    }
#endif /* MOCHA */

        // else must be an OLE stream exit
        if(iStatus != MK_DATA_LOADED)   {
            //  Load error.
            pItem->m_bBroken = TRUE;
        }
        else if(pUrl->server_status != 0 && pUrl->server_status / 100 != 2 && pUrl->server_status / 100 != 3 && iStatus == MK_DATA_LOADED)  {
            //  Server error.
            pItem->m_bBroken = TRUE;
        }

        //  If the item isn't broken, we can load it up.
        if(pItem->m_bBroken == FALSE)   {
            SetCursor(AfxGetApp()->LoadStandardCursor(IDC_WAIT));
			if(FALSE == pItem->CreateFromFile(pItem->m_csFileName)) {
				//  Couldn't create for some reason!
				pItem->m_bBroken = TRUE;
			}
            SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
        }
        pItem->m_bLoading = FALSE;

        //  Get the width and height out of our newly created item, if it isn't broken.
	    //	We need to do this two different ways, depending on what type of context we
	    //		are in.
	    CSize csExtents;
	    //	New way.
	    CDCCX *pCX = VOID2CX(pContext->fe.cx, CDCCX);
	    if(pItem->m_bBroken == FALSE)	{
	    	pItem->GetExtent(&csExtents);
	    	csExtents.cx = CASTINT(pCX->Metric2TwipsX(csExtents.cx));
	    	csExtents.cy = CASTINT(pCX->Metric2TwipsY(csExtents.cy));
	    }
	    else	{
	    	LTRB Rect;
	    	int32 x, y;
//	    	pCX->DisplayIcon(Rect.left, Rect.right, IL_IMAGE_BAD_DATA, &x, &y);
            pCX->GetIconDimensions(&x, &y, IL_IMAGE_BAD_DATA);
	    	csExtents.cx = CASTINT(x);
	    	csExtents.cy = CASTINT(y);
	    }
        //  Need to flush all delayed display, and blocked layout.
        //  Do all blocks.
        POSITION rIndex = pItem->m_cplUnblock.GetHeadPosition();
        LO_EmbedStruct *pLayoutData = NULL;
        while(rIndex != NULL && iStatus != MK_INTERRUPTED)   {
            pLayoutData = (LO_EmbedStruct *)pItem->m_cplUnblock.GetNext(rIndex);
			if ((pEmbeddedApp->type ==  NP_OLE) && pItem->m_lpObject) {
				if ( pLayoutData->width)
					csExtents.cx = pLayoutData->width; 
				if ( pLayoutData->height)
					csExtents.cy = pLayoutData->height; 
				pLayoutData->width = csExtents.cx;
				pLayoutData->height = csExtents.cy;

			}
            LO_ClearEmbedBlock(ABSTRACTCX(pContext)->GetDocumentContext(), pLayoutData);        
        }
        pItem->m_cplUnblock.RemoveAll();

#ifdef LAYERS
        rIndex = pItem->m_cplElements.GetHeadPosition();
        while (rIndex != NULL) {
            pLayoutData = (LO_EmbedStruct *)pItem->m_cplElements.GetNext(rIndex);
            // An OLE container is windowless until it is activated.
            LO_SetEmbedType(pLayoutData, PR_FALSE);
        }
#endif // LAYERS
        
        //  Do all needed display.
        rIndex = pItem->m_cplDisplay.GetHeadPosition();
        while(rIndex != NULL)   {
            pLayoutData = (LO_EmbedStruct *)pItem->m_cplDisplay.GetNext(rIndex);

			if ((pEmbeddedApp->type ==  NP_OLE) && pItem->m_lpObject) {
				if ( pLayoutData->width )
					csExtents.cx = pLayoutData->width; 
				if ( pLayoutData->height)
					csExtents.cy = pLayoutData->height;
				pLayoutData->width = csExtents.cx;
				pLayoutData->height = csExtents.cy;
			}
#ifdef LAYERS
            if (pContext->compositor) {
                XP_Rect rect;
                
                CL_GetLayerBbox(pLayoutData->layer, &rect);
                CL_UpdateLayerRect(CL_GetLayerCompositor(pLayoutData->layer),
                                   pLayoutData->layer, &rect, PR_FALSE);
            }
            else
#endif /* LAYERS */
                pContext->funcs->DisplayEmbed(pContext, FE_VIEW, pLayoutData);

        }
        pItem->m_cplDisplay.RemoveAll();
    }

    //  And well, hey, get rid of the url.
    NET_FreeURLStruct(pUrl);
}

static void wfe_PluginStream(URL_Struct *pUrlData, MWContext *pContext)
{
    NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pUrlData->fe_data;

	ASSERT(pEmbeddedApp);

	// Make sure the type is set properly
    pEmbeddedApp->type = NP_Plugin;

    //  Need to flush all delayed display, and blocked layout.
    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;

	ASSERT(pItem);

	if (!pItem->m_cplUnblock.IsEmpty()) {
		// There had better only be one item here
		ASSERT(pItem->m_cplUnblock.GetCount() == 1);

		POSITION		rIndex = pItem->m_cplUnblock.GetHeadPosition();
        LO_EmbedStruct*	pLayoutData = (LO_EmbedStruct *)pItem->m_cplUnblock.GetNext(rIndex);

		// Unblock layout
        LO_ClearEmbedBlock(ABSTRACTCX(pContext)->GetDocumentContext(), pLayoutData);        
		pItem->m_cplUnblock.RemoveAll();
	}

    //  Display the plugin if necessarily
	if (!pItem->m_cplDisplay.IsEmpty()) {
		// There had better only be one item here
		ASSERT(pItem->m_cplDisplay.GetCount() == 1);

		POSITION		rIndex = pItem->m_cplDisplay.GetHeadPosition();
        LO_EmbedStruct*	pLayoutData = (LO_EmbedStruct *)pItem->m_cplDisplay.GetNext(rIndex);

#ifdef LAYERS
        int32 lXSave, lYSave;
        XP_Rect rect;
        
        // I don't like the fact that this code has to be here. At some
        // level, it goes against the rule that all drawing has to be
        // driven by the compositor through layout. The easy fix is that
        // we do the same coordinate space conversion that's done in
        // layout.  -- Vidur
        if (pContext->compositor) {
            /* Convert layer-relative coordinates for element to document
               coordinates, since that's what the FE uses. */
            rect.top = rect.left = rect.right = rect.bottom = 0;
            CL_LayerToWindowRect(pContext->compositor, pLayoutData->layer, &rect);
            CL_WindowToDocumentRect(pContext->compositor, &rect);
            
            /* Save old, layer-relative coordinates */
            lXSave = pLayoutData->x;
            lYSave = pLayoutData->y;
            
            /* Temporarily shift element to document coordinates */
            pLayoutData->x = rect.left - pLayoutData->x_offset;
            pLayoutData->y = rect.top - pLayoutData->y_offset;
        }
#endif /* LAYERS */

		// Display the plugin
        pContext->funcs->DisplayEmbed(pContext, FE_VIEW, pLayoutData);

#ifdef LAYERS
        if (pContext->compositor) {
            pLayoutData->x = lXSave;
            pLayoutData->y = lYSave;
        }
#endif /* LAYERS */

		pItem->m_cplDisplay.RemoveAll();
    }
}

BOOL WildMime(const char *pOne, const char *pTwo)	{
//	Purpose:	Compare two mime types to see if they can be considered
//					equivalent.
//	Arguments:	csOne	A fully qualified mime type, no wilds.
//				csTwo	A mime type, can be wild, or subtype wild.
//	Returns:	BOOL	TRUE	A match.
//						FALSE	No match.
//	Comments:	Mime types are case insensitive.
//	Revision History:
//		01-04-95	created GAB
//

	//	First, just get rid of any wilds right now.
	if(*pTwo == '*')	{
		return(TRUE);
	}
	
	//	Okay, only have subtype wilds left, split the mime types up
	//		into minor, major parts.
	char *p1 = strdup(pOne);
	char *pmin1 = strchr(p1, '/');
	if(pmin1) {
		*pmin1 = '\0';
		pmin1++;
	} else {
		pmin1 = p1;
	}
	
		
	char *p2 = strdup(pTwo);
	char *pmin2 = strchr(p2, '/');
	if(pmin2) {
		*pmin2 = '\0';
		pmin2++;
	} else {
	 	pmin2 = p2;	
	}	           
	
	//	If majors don't match, there is no hope.
	if(stricmp(p1, p2) != 0)	{
		free(p1);
		free(p2);
		return(FALSE);
	}
	
	//	Okay, let's get rid of sub type wilds right now.
	if(*pmin2 == '*')	{
		free(p1);
		free(p2);
		return(TRUE);
	}
	
	//	See if the minors match up.
	BOOL bRetVal = FALSE;
	if(stricmp(pmin1, pmin2) == 0)	{
		bRetVal = TRUE;
	}
	
	free(p1);
	free(p2);
	return(bRetVal);
}


static BOOL wfe_IsRegisteredForPlugin(int iFormatOut, URL_Struct *pUrlStruct, MWContext *pContext)
{
	//	Find the callers mime/type in the iFormatOut registry list,
	//  and return true if found.
	CString csMimeType = pUrlStruct->content_type;
	
	//	Find the relevant mime type in our list.
	//	There should always be a wild on the end of the list, but if not, duh.
	XP_List *list = NET_GetRegConverterList(iFormatOut);
    ContentTypeConverter *pConv;

	while(pConv = (ContentTypeConverter *)XP_ListNextObject(list))
	{
		
		//	Do a wild compare on the mime types
		if(WildMime(csMimeType, pConv->format_in))
		{
            //  May have found an appropriate converter.

            //  Only when the viewer is not automated,
            //  and the mime types are a case insensitive
            //  match, return TRUE.
			// ZZZ: Make sure it's a plug-in and not an automated viewer.
			// We're doing it this demented way because pConv->bAutomated is
			// getting stomped and points to garbage
			if ((pConv->bAutomated == FALSE) && NPL_FindPluginEnabledForType(pConv->format_in)) {
				// only check for can handle by OLE when there is no plugin register for
				// the mine type.
				// Find out can we handle by OLE.
				if (strcmp(pConv->format_in, "*") == 0) 
					/* there previously was a call to FE_FileType here, but it is clearly
					unnecessary given the check of fe_CanHandlebyOLE we've added.  byrd.
					reminder - we should overhaul/remove FE_FileType and it's other call.
					&& 
					FE_FileType(pUrlStruct->address, pUrlStruct->content_type,
									pUrlStruct->content_encoding))
									*/
				{
					if(iFormatOut == FO_EMBED){
						/* don't have to worry about FO_CACHE_AND_EMBED since cache bit cleared by NET_CacheConverter */
						/* also, don't want to interfere w/ full-page case... */
						char* ext[1];
						ext[0] = FE_FindFileExt(pUrlStruct->address);

						if(ext[0] && fe_CanHandleByOLE(ext,1))
							return FALSE;
						else
							return TRUE;
					}

					else return FALSE;
				}
				else
                    return TRUE;
            }
            //  Only when the viewer is not automated,
            //  and the handler is for wildcard MIME type,
            //  and OLE doesn't want it, return TRUE.
			// ZZZ: See above comment
            if ((pConv->bAutomated == FALSE) && XP_STRCMP(pConv->format_in, "*") == 0 &&
				NPL_FindPluginEnabledForType("*")) {
                // the following code is copied from EmbedStream(), OLE related stuff
                // BUG: this code needs to be shared code!

                // extract the extension of the file name
                char aExt[_MAX_EXT];
                size_t stExt = 0;
                DWORD dwFlags = EXT_NO_PERIOD;
                
#ifdef XP_WIN16
                dwFlags |= EXT_DOT_THREE;
#endif
                aExt[0] = '\0';
                stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pUrlStruct->address, pUrlStruct->content_type);
                CString csFinalExtension = aExt;

                //  Check to see if the embedded file matches any known extensions.
                //  If not, then consider the file of no use to the user.
	            //	Use new way if we are in a different style of context.
                if(wfe_IsExtensionRegistrationValid(csFinalExtension, ABSTRACTCX(pContext)->GetDialogOwner(), FALSE) == FALSE) {
                    return TRUE;
                }		
            }
		}
	}
    return FALSE;
}

NET_StreamClass *EmbedStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext)
{
    NPEmbeddedApp* pEmbeddedApp = NULL;
    CNetscapeCntrItem* pItem = NULL;
    NET_StreamClass * pStream = NULL;

    if(wfe_IsRegisteredForPlugin(iFormatOut, pUrlData, pContext)) {
        if(!ABSTRACTCX(pContext)->IsPrintContext()) {
			if (iFormatOut == FO_EMBED) {
				// NPL_NewEmbedStream() sets pUrlData->fe_data to NPEmbeddedApp*, so
				// it is not valid until NPL_NewEmbedStream() returns
				pStream = NPL_NewEmbedStream((FO_Present_Types)iFormatOut,
					pDataObj, pUrlData, pContext);
				
			} else if (iFormatOut == FO_PRESENT) {
				// NPL_NewPresentStream() sets pUrlData->fe_data to NPEmbeddedApp*, so
				// it is not valid until NPL_NewPresentStream() returns
				pStream = NPL_NewPresentStream((FO_Present_Types)iFormatOut,
					pDataObj, pUrlData, pContext);
				
			} else {
				ASSERT(FALSE);
				return NULL;
			}

            // If we fail to load for some reason, show a broken icon.
            if(!pStream) {
                if(pItem)
                    pItem->m_bBroken = TRUE;
                return NULL;
            }
            pEmbeddedApp = (NPEmbeddedApp*)pUrlData->fe_data;
            if(pEmbeddedApp)
                pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
        }

        // pDataObj gets its fe_data member written by libplugin in
        // the full screen case
		//
		// It's possible that the context is different. This happens for full-page
		// plug-ins when we decide to display the plugin in it's own window
		if (pItem && pItem->GetDocument()) {
			CDCCX*	pDC = pItem->GetDocument()->GetContext();

			if (pDC && pDC->GetContext() != pContext) {
				TRACE0("Context changed in EmbedStream()!\n");
				pContext = pDC->GetContext();
			}
		}

        wfe_PluginStream(pUrlData, pContext);
        return pStream;
    }

	// if the MIME type isn't registered to a plugin,
	// assume it must be handled by OLE
    pEmbeddedApp = (NPEmbeddedApp*)pUrlData->fe_data;
    if(pEmbeddedApp == NULL) // our OLE support doesn't do full page
        return NULL;

	// Explicitly type it as an embedded OLE object
	pEmbeddedApp->type = NP_OLE;
    pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;
	pItem->m_bIsOleItem = TRUE;

    //  Formulate a file name that will suckle data from the net.
    char *pFormulateName = fe_URLtoLocalName(pItem->m_csAddress, pUrlData ? pUrlData->content_type : NULL);
    char *pLocalName = NULL;
    if(((CNetscapeApp *)AfxGetApp())->m_pTempDir != NULL && pFormulateName != NULL) {
        StrAllocCopy(pLocalName, ((CNetscapeApp *)AfxGetApp())->m_pTempDir);
        StrAllocCat(pLocalName, "\\");
        StrAllocCat(pLocalName, pFormulateName);

        //  If this file exists, then we must attempt another temp file.
        if(-1 != _access(pLocalName, 0))    {
            //  Retain the extension.
            char aExt[_MAX_EXT];
            size_t stExt = 0;
            DWORD dwFlags = 0;
            
#ifdef XP_WIN16
            dwFlags |= EXT_DOT_THREE;
#endif
            aExt[0] = '\0';
            stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pUrlData->address, pUrlData->content_type);

            if(pLocalName) {
                XP_FREE(pLocalName);
                pLocalName = NULL;
            }
            pLocalName = WH_TempFileName(xpTemporary, "E", aExt);
        }
    }
    else    {
        //  Retain the extension.
        char aExt[_MAX_EXT];
        size_t stExt = 0;
        DWORD dwFlags = 0;
        
#ifdef XP_WIN16
        dwFlags |= EXT_DOT_THREE;
#endif
        aExt[0] = '\0';
        stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pUrlData->address, pUrlData->content_type);

        if(pLocalName) {
            XP_FREE(pLocalName);
            pLocalName = NULL;
        }
        pLocalName = WH_TempFileName(xpTemporary, "E", aExt);
    }

    if(pFormulateName != NULL)  {
        XP_FREE(pFormulateName);
        pFormulateName = NULL;
    }

    //  Correctly extract the extension from the created file name.
    char aExt[_MAX_EXT];
    size_t stExt = 0;
    DWORD dwFlags = EXT_NO_PERIOD;
    
#ifdef XP_WIN16
    dwFlags |= EXT_DOT_THREE;
#endif
    aExt[0] = '\0';
    stExt = EXT_Invent(aExt, sizeof(aExt), dwFlags, pLocalName, pUrlData->content_type);
    CString csFinalExtension = aExt;

    //  Check to see if the embedded file matches any known extensions.
    //  If not, then consider the file of no use to the user.
	//	Use new way if we are in a different style of context.
    if(pEmbeddedApp->type == NP_OLE && pItem->m_isFullPage) {
		//; do nothing for full page OLE.  Because the security dialog already shown on external_viewer_disk_stream.
	}
	else {
		if(wfe_IsExtensionRegistrationValid(csFinalExtension, ABSTRACTCX(pContext)->GetDialogOwner(), TRUE) == FALSE) {
			//  Broken....
			XP_FREE(pLocalName);
			pItem->m_bBroken = TRUE;
			return(NULL);
		}
	}
    //  Open up the file for writing.
    pItem->m_csFileName = pLocalName;
    XP_FREE(pLocalName);

    CFileException cfe;
    if(FALSE == pItem->m_cfOutput.Open(pItem->m_csFileName, CFile::modeCreate | CFile::modeWrite, &cfe))    {
        //  Couldn't open.
        pItem->m_bBroken = TRUE;
        return(NULL);
    }

    //  Create the stream class that will handle the download.
    //  Send the embed along for the download.
    pItem->m_bLoading = TRUE;
    return(NET_NewStream("EmbeddedItemCheeseSpankDior",
        EmbedWrite,
        EmbedComplete,
        EmbedAbort,
        EmbedReady,
        (void *)pUrlData,
        pContext));
}

unsigned int EmbedReady(NET_StreamClass *stream) {
	void *pDataObj=stream->data_object;	
	// pDataObj is an URL struct
	if(wfe_IsTypePlugin((NPEmbeddedApp*)((URL_Struct*)pDataObj)->fe_data))
        return NPL_WriteReady(stream);

    //  The the netowrk library how much we're willing to write to our new embedded item.
    return(MAX_WRITE_READY);
}
 
int EmbedWrite(NET_StreamClass *stream, const char *pWriteData, int32 lLength)   {
	void *pDataObj=stream->data_object;	
	// pDataObj is an URL struct
	if(wfe_IsTypePlugin((NPEmbeddedApp*)((URL_Struct*)pDataObj)->fe_data))
        return NPL_Write(stream, (unsigned char *)pWriteData, lLength);

    //  Serialize some more information for the embedded item.
    CNetscapeCntrItem *pItem = wfe_GetCntrPtr(pDataObj);
    TRY {   
        pItem->m_cfOutput.Write(pWriteData, CASTUINT(lLength));
    }
    CATCH(CFileException, e)    {
        //  Couldn't write for some reason.
        pItem->m_bBroken = TRUE;
        return(MK_DISK_FULL);
    }
    END_CATCH

    return(MK_DATA_LOADED);
}

void EmbedComplete(NET_StreamClass *stream)  {
	void *pDataObj=stream->data_object;	
	// pDataObj is an URL struct
	if(wfe_IsTypePlugin((NPEmbeddedApp*)((URL_Struct*)pDataObj)->fe_data))
    {
        NPL_Complete(stream);
        return;
	}
    //  The load is complete.
    //  Close our file.
    CNetscapeCntrItem *pItem = wfe_GetCntrPtr(pDataObj);
    TRY {
        pItem->m_cfOutput.Close();
    }
    CATCH(CFileException, e)    {
        //  Some error.
        pItem->m_bBroken = TRUE;
    }
    END_CATCH
}

void EmbedAbort(NET_StreamClass *stream, int iStatus)    {
	void *pDataObj=stream->data_object;	
	// pDataObj is an URL struct
	if(wfe_IsTypePlugin((NPEmbeddedApp*)((URL_Struct*)pDataObj)->fe_data))
    {
        NPL_Abort(stream, iStatus);
        return;
	}
    //  The load was aborted.
    //  Do a normal close, then mark ourselves as broken.
    EmbedComplete(stream);

    CNetscapeCntrItem *pItem = wfe_GetCntrPtr(pDataObj);
    pItem->m_bBroken = TRUE;
}
};

BOOL wfe_IsExtensionRegistrationValid(CString& csExtension, CWnd *pWnd, BOOL bAskDialog) {
//	Purpose:    Determine if an extension will present the user with a useful embedded item.
//	Arguments:  csExtension The extension of the data file.
//              pWnd        The calling window
//	Returns:    BOOL    TRUE    The extension has an application present that can handle it.
//                      FALSE   Either the extension is not presently registered,
//                              the registered viewer is no longer present locally, or
//                              the application isn't trusted (CanSpawn).
//	Comments:   Troy's handywork.
//	Revision History:
//      02-07-95    created GAB
//      04-10-95    modified to check and see if execution of the application has
//                      been cleared by the user.
//

    char szExt[_MAX_EXT], szValue[_MAX_PATH];
    long cb;
    OFSTRUCT of;

    //  First, try the system registry.
    wsprintf(szExt, ".%s", (LPCSTR)csExtension);
    cb = sizeof(szValue);
    if(RegQueryValue(HKEY_CLASSES_ROOT, szExt, szValue, &cb) == ERROR_SUCCESS)  {
        CString strKey = szValue;
        strKey += "\\shell\\open\\command";
        cb = sizeof(szValue);
        if(RegQueryValue(HKEY_CLASSES_ROOT, strKey, szValue, &cb) == ERROR_SUCCESS) {
            //  Just use the app name, ignore switches.
//            char *pAppName = strtok(szValue, " ");

			// mwh this is becuase in Win 32 directory name can have space. 
			// still need to fix the cause that the directory has '.'

            char *pAppName = strchr(szValue, '.');
			if (pAppName) {
				char *next = strchr(pAppName, ' ');
				*next = '\0';  // NULL terminated the string.

				// mhw to get around the problem, that some shell command have "".
				if (*(next-1) == '"') *(next-1) = 0; 
				if (szValue[0] == '"')
					pAppName = &szValue[1];
				else
					pAppName = szValue;
				if(pAppName)    {
					if(OpenFile(pAppName, &of, OF_EXIST) != HFILE_ERROR)    {
						//  See if we can spawn this app.
						if(bAskDialog == TRUE)
							return(((CNetscapeApp *)AfxGetApp())->m_pSpawn->CanSpawn(CString(pAppName), pWnd));
						else
							return TRUE;
					}
				}
            }
        }
    }

    //  Second try win.ini win16 associations.
    if(::GetProfileString("Extensions", csExtension, "", szValue, sizeof(szValue))) {
        char *pAppName = strtok(szValue, " ");
        if(pAppName != NULL)    {
            if(OpenFile(pAppName, &of, OF_EXIST) != HFILE_ERROR)    {
                //  see if we can spawn this app.
                if(bAskDialog == TRUE)
                    return(((CNetscapeApp *)AfxGetApp())->m_pSpawn->CanSpawn(CString(pAppName), pWnd));
                else
                    return TRUE;
            }
        }
    }
    //  Neither will work, nothing will work.
    return(FALSE);
}
