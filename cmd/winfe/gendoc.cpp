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

// gendoc.cpp : implementation file
//

#include "stdafx.h"

#include "cntritem.h"
#include "srvritem.h"
#include "feembed.h" 
#include "np.h"
#include "nppg.h"
#include "plginvw.h"
#include "edt.h"
#include "il_icons.h"
#include "edframe.h"
#include "ipframe.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#ifdef XP_WIN16
#define T2COLE(str) str
#define LPCOLESTR LPCTSTR
#endif
/////////////////////////////////////////////////////////////////////////////
// CGenericDoc

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CGenericDoc, COleServerDoc)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

//  Default extent values.
#define HIY 7000
#define HIX 7000


LPUNKNOWN AFXAPI _myQueryInterface(LPUNKNOWN lpUnknown, REFIID iid)
{
	ASSERT(lpUnknown != NULL);

	LPUNKNOWN lpW = NULL;
	if (lpUnknown->QueryInterface(iid, (void**)&lpW) != S_OK)
		return NULL;

	return lpW;
}
#define QUERYINTERFACE(lpUnknown, iface) \
	(iface*)_myQueryInterface(lpUnknown, IID_##iface)

CGenericDoc::CGenericDoc()
{
	// In case someone forgets to tell us.
	m_pContext = NULL;

    //  Haven't set a document file yet.
    m_bOpenDocumentFileSet = FALSE;

	//	We can close the document.
	m_bCanClose = TRUE;

    //  Default extents.
    m_csViewExtent = CSize(HIX, HIY);
    m_csDocumentExtent = CSize(HIX, HIY);

}

CGenericDoc::~CGenericDoc()
{

	//	When the document goes down, it's a good idea to let the context
	//		know it's no longer there.
	if(GetContext() != NULL)	{
		GetContext()->ClearDoc();
	}

	//	Get rid of the context if around.
	if(GetContext() && GetContext()->IsDestroyed() == FALSE)	{
		GetContext()->DestroyContext();
	}
}


BEGIN_MESSAGE_MAP(CGenericDoc, COleServerDoc)
    //{{AFX_MSG_MAP(CGenericDoc)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_OLE_UPDATE, OnOleUpdate)
	ON_UPDATE_COMMAND_UI(ID_OLE_UPDATE, OnUpdateOleUpdate)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



/////////////////////////////////////////////////////////////////////////////
// CGenericDoc diagnostics

#ifdef _DEBUG
void CGenericDoc::AssertValid() const
{
    COleServerDoc::AssertValid();
}

void CGenericDoc::Dump(CDumpContext& dc) const
{
    COleServerDoc::Dump(dc);
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CGenericDoc serialization

void CGenericDoc::Serialize(CArchive& ar)
{
	TRACE("CGenericDoc::Serialize called\n");

    BOOL bCanSerialize = TRUE;
    if(ar.IsLoading())  {
#ifdef XP_WIN32
        //  Determine name of actual document.
        CFile *pFile = ar.GetFile();
        CString csFile;
        if(pFile)   {
            csFile = pFile->GetFilePath();
        }

        //  Determine if this is the same file that was passed into the
        //      OnOpenDocument function.
        //  If so, then we can not read our OLE format.
        bCanSerialize = (csFile.CompareNoCase(m_csOpenDocumentFile) != 0);

        //  However, if they're both empty, then we need to allow this.
        if(csFile.IsEmpty() && m_csOpenDocumentFile.IsEmpty())  {
            bCanSerialize = TRUE;
        }
        TRACE("%d = !(%s==%s)\n", bCanSerialize, (const char *)csFile, (const char *)m_csOpenDocumentFile);
#else
        //  16 bit can not turn a file handle into a file name.
        //  If our document name is set, then say we can't serialize.
        bCanSerialize = !(m_bOpenDocumentFileSet && !m_csOpenDocumentFile.IsEmpty());
        TRACE("%d = !(%d && !%d)\n", m_bOpenDocumentFileSet, !m_csOpenDocumentFile.IsEmpty());
#endif
    }

    //  We can only serialize if we're not looking at a local file which we've opened up.
    if(bCanSerialize)   {
	    //	Fleshed out for OLE server work.
	    //	The below is the common implementation that should work across the
	    //		board for all versions of the navigator.
	    //	All it does is either read in or write out a URL.
	    if(GetContext() && GetContext()->IsDestroyed() == FALSE)	{
	        if (ar.IsStoring())
	        {
			    TRACE("Storing\n");
			    //	Just shove our current URL into the file.
			    //	Get the current history entry.
			    History_entry *pHist = SHIST_GetCurrent(&(GetContext()->GetContext()->hist));
			    if(pHist != NULL && pHist->address != NULL)	{
				    CString csAddress = pHist->address;
				    ar << csAddress;
				    TRACE("URL is %s\n", (const char *)csAddress);
			    }
			    else	{
				    TRACE("no history!\n");
				    CString csEmpty;
				    ar << csEmpty;
			    }
	        }
	        else
	        {
			    TRACE("Reading\n");

			    //	Pretty much just read this in as internet shortcut
			    //		format and load it.
			    CString csLoadMe;
			    ar >> csLoadMe;

			    //	Do it.
			    TRACE("URL is %s\n", (const char *)csLoadMe);
			    GetContext()->NormalGetUrl(csLoadMe);
	        }
	    }
	    else	{
		    TRACE("no context!\n");
	        if (ar.IsStoring())
	        {
			    TRACE("Storing\n");
			    //	Hope that ephemeral data were cached, otherwise, no real
			    //		harm done.
			    ar << m_csEphemeralHistoryAddress;
	        }
	        else
	        {
			    TRACE("Reading\n");
			    CString csDontcare;
			    ar >> csDontcare;
	        }
	    }

	    //	Next in line should be a string identifying the client which wrote
	    //		out the information.
        //  ALL NUMBERS TO BE CONVERTED TO NETWORK BYTE ORDER, SO WORKS ACROSS PLATFORMS.
	    CString csNetscapeVersion;
	    if(ar.IsStoring())	{
		    //	Write out our document version number.
		    //	This is initially 2.0
		    ar << theApp.ResolveAppVersion();
		    TRACE("App version is %s\n", (const char *)theApp.ResolveAppVersion());

		    //	Write out any other information for a particular version here,
		    //		prepended by the amount of total bytes to be written.
		    //	Hold all integer values (all XP values) to a size that will
		    //		translate between win16 and win32.
		    TRACE("Writing version specific information.\n");

            //  Figure up the size of extra info we'll be writing out.
            u_long arExtraBytes = 0;

            //  3.0 beta 6 has some extra information
            arExtraBytes += sizeof(u_long); //  extent.cx
            arExtraBytes += sizeof(u_long); //  extent.cy
            arExtraBytes += sizeof(u_long); //  extent.cx
            arExtraBytes += sizeof(u_long); //  extent.cy

		    //	For version 2.0, there was no extra information.
		    ar << htonl(arExtraBytes);

            //  Now, begin writing out extra information.

            //  3.0 beta 6
            TRACE("3.0 beta 6 and later information being written\n");
            ar << htonl((u_long)m_csViewExtent.cx);
            ar << htonl((u_long)m_csViewExtent.cy);
            TRACE("Write cx=%lu cy=%lu\n", (uint32)m_csViewExtent.cx, (uint32)m_csViewExtent.cy);
            ar << htonl((u_long)m_csDocumentExtent.cx);
            ar << htonl((u_long)m_csDocumentExtent.cy);
            TRACE("Write cx=%lu cy=%lu\n", (uint32)m_csDocumentExtent.cx, (uint32)m_csDocumentExtent.cy);
	    }
	    else	{
		    //	Read in our document version number.
		    ar >> csNetscapeVersion;
		    TRACE("App version is %s\n", (const char *)csNetscapeVersion);

		    //	Next, read in the amount of bytes that are stored.
		    u_long lBytes;
		    ar >> lBytes;
            lBytes = ntohl(lBytes);

		    if(lBytes != 0)	{
			    //	Now, depending on which version we're reading from,
			    //		figure out the information in the file if we understand
			    //		that particular version's file format.
			    //	2.0 won't understand anything, so just read in the number of
			    //		extra bytes and continue.
			    //	Let the caller of serialize handle thrown exceptions.
			    TRACE("Reading version specific information of %lu bytes.\n", lBytes);
			    char *pBuf = new char[lBytes];
			    if(pBuf == NULL)	{
				    AfxThrowMemoryException();
			    }
			    ar.Read((void *)pBuf, CASTUINT(lBytes));

                //  Use and increment this pointer appropriately to read in
                //      extra information.
                char *va_start = pBuf;

                //  If and only if there are extra bytes, decide what was written out.
                //  In 3.0 beta 6, we wrote out two uint32's first.
                u_long arVersion30b6 = 0;
                arVersion30b6 += sizeof(u_long);
                arVersion30b6 += sizeof(u_long);
                arVersion30b6 += sizeof(u_long);
                arVersion30b6 += sizeof(u_long);

                //  Read in this information, otherwise, use a default.
                if(lBytes >= arVersion30b6) {
                    TRACE("3.0 beta 6 information being retrieved\n");
                    m_csViewExtent.cx = CASTINT(ntohl(*((u_long *)va_start)));
                    va_start += sizeof(u_long);
                    m_csViewExtent.cy = CASTINT(ntohl(*((u_long *)va_start)));
                    va_start += sizeof(u_long);
                    TRACE("Read cx=%lu cy=%lu\n", (uint32)m_csViewExtent.cx, (uint32)m_csViewExtent.cy);
                    m_csDocumentExtent.cx = CASTINT(ntohl(*((u_long *)va_start)));
                    va_start += sizeof(u_long);
                    m_csDocumentExtent.cy = CASTINT(ntohl(*((u_long *)va_start)));
                    va_start += sizeof(u_long);
                    TRACE("Read cx=%lu cy=%lu\n", (uint32)m_csDocumentExtent.cx, (uint32)m_csDocumentExtent.cy);
                }
                else    {
                    TRACE("Using default 3.0 beta 6 information\n");
                    m_csViewExtent = CSize(HIX, HIY);
                    m_csDocumentExtent = CSize(HIX, HIY);
                }

                delete[] pBuf;
		    }
	    }

        //  Calling the base class CGenericDoc enables serialization
        //      of the conainer document's COleclientItem objects.
	    TRACE("Calling base serialize\n");
        COleServerDoc::Serialize(ar);
    }
}

UINT GetChildID()
{
    static UINT s_uChildID;
    return s_uChildID++;
}

//  Get the size of an embedded item.
void CGenericDoc::GetEmbedSize(MWContext *pContext, LO_EmbedStruct *pLayoutData, NET_ReloadMethod Reload) {
    //  First, see if we've already got what Layout is asking for.
    POSITION rIndex = GetStartPosition();
    char *pLayoutAddress = (char *)pLayoutData->embed_src;
    CNetscapeCntrItem *pItem = NULL;
    CString csLoad = pLayoutAddress;

    while(rIndex != NULL)   {
        pItem = (CNetscapeCntrItem *)GetNextClientItem(rIndex);
        if(pItem == NULL)   {
            break;
        }

        //  Compare the address to the one layout has.
        if(pItem->m_csAddress == csLoad)    {
            //  One in the same. If this is an OLE container item, then just
			// reuse the pItem. This allows us to use the existing temp file
			// and to avoid downloading the file multiple times
			//
			// Plugins work differently. A new plugin instance will be created
			// for each embed, and each plugin instance will have a stream associated
			// with it. Because the plugin data is cached, we won't be reloading
			// multiple times
			//
			// Only reuse this pItem if it corresponds to an OLE object. We can
			// tell by looking at the first NPEmbeddedApp. Unfortunately this isn't
			// very reliable, because the previous embed may still be untyped
			POSITION rIndex = pItem->m_cplElements.GetHeadPosition();
			ASSERT(rIndex != NULL);

			if (rIndex) {
				LO_EmbedStruct* pLayoutData = (LO_EmbedStruct *)pItem->m_cplElements.GetNext(rIndex);

				ASSERT(pLayoutData && pLayoutData->type == LO_EMBED);
				if (pLayoutData) {
					NPEmbeddedApp* pEmbeddedApp = (NPEmbeddedApp*)pLayoutData->FE_Data;

					ASSERT(pEmbeddedApp);
					if (pEmbeddedApp && pEmbeddedApp->type == NP_OLE)
						break;  // it's okay to reuse this pItem
				}
			}
        }

        //  Not a match, reset the value.
        pItem = NULL;
    }

    //  See if we have a match. Note that this should never happen with plugins
	// (OLE only)
    if(pItem != NULL)   {
        //  We have a match.
        //  Add this layout item to the number of layout items accessing us.
        pItem->m_cplElements.AddTail(pLayoutData);

        // create and init new structures for managing an embedded plugin
	    NPEmbeddedApp *pEmbeddedApp = NULL;
       
        NPWindow *pAppWin = XP_NEW_ZAP(NPWindow);
        if(pAppWin == NULL)
        {
            return;
        }

		if(!(pEmbeddedApp = XP_NEW_ZAP(NPEmbeddedApp)))
        {
            XP_FREE(pAppWin);
		    return;
        }

		pEmbeddedApp->next = pContext->pluginList;
		pContext->pluginList = pEmbeddedApp;
        pEmbeddedApp->type  = NP_Untyped; // figure it out in EmbedStream
        pEmbeddedApp->fe_data = (void *)pItem;
        pEmbeddedApp->wdata = pAppWin;

        //  Go ahead and assign it into the layout data for use in other functions.
        pLayoutData->FE_Data = (void *)pEmbeddedApp;
        pItem->m_iLock++;

        //  Is layout blocking for this information?
        if(pLayoutData->width == 0 || pLayoutData->height == 0) {

            if(pItem->m_bLoading == TRUE)   {
                //  Add this layout element to those that will be unblocked once loaded.
                pItem->m_cplUnblock.AddTail(pLayoutData);
                return;
            }

            //  We have to switch here to handle the old way of doing things, and the new.
            //  Base class which handles embeds is CDCCX.
            CDCCX *pCX = VOID2CX(pContext->fe.cx, CDCCX);

            //  Go ahead and set the size of the item.
            if(pItem->m_bDelayed == TRUE)   {
                LTRB Rect;
                pCX->DisplayIcon(Rect.left, Rect.top, IL_IMAGE_DELAYED);
            }
            else if(pItem->m_bBroken == TRUE)   {
                LTRB Rect;
                pCX->DisplayIcon(Rect.left, Rect.top, IL_IMAGE_BAD_DATA);
            }
            else    {
                CSize csExtents;
                pItem->GetExtent(&csExtents);
				csExtents.cx = pCX->Metric2TwipsX(csExtents.cx);
				csExtents.cy = pCX->Metric2TwipsY(csExtents.cy);

				if ( pLayoutData->width )
					csExtents.cx = pLayoutData->width;
				if ( pLayoutData->height )
					csExtents.cy = pLayoutData->height; 
				pLayoutData->width = csExtents.cx;
				pLayoutData->height = csExtents.cy;
            }

            return;
        }
        else    {
            if(pItem->m_bLoading == TRUE)   {
                //  We'll need to update this once the load is completed.
                pItem->m_cplDisplay.AddTail(pLayoutData);
                return;
            }
        }
    }
    else    {
		BOOL			bPrinting = GetContext()->IsPrintContext();
        NPEmbeddedApp* 	pEmbeddedApp;

		CString theURL;
		const char * tempURL;
	    PA_LOCK(tempURL, const char*, pLayoutData->embed_src);
		theURL = tempURL;
		BOOL fullPageOLE = FALSE;
		int index;
		if ((index = theURL.Find( "internal-ole-viewer:")) > 0) {
			PA_UNLOCK(pLayoutData->embed_src); 
			index += strlen("internal-ole-viewer:");
			PA_FREE(pLayoutData->embed_src);
			int newEmbed_len = (theURL.GetLength() - index) + 1;
			pLayoutData->embed_src = (PA_Block)PA_ALLOC(newEmbed_len);
			char* str;
			PA_LOCK(str, char *, pLayoutData->embed_src);
			tempURL = theURL;
			strncpy(str, tempURL+index, newEmbed_len - 1);
			str[newEmbed_len - 1] = 0;  // null terminated.
			pLayoutAddress = (char *)pLayoutData->embed_src;
			PA_UNLOCK(pLayoutData->embed_src);
			fullPageOLE = TRUE;

		}
		else
			PA_UNLOCK(pLayoutData->embed_src); 


        // Create and init new structures for managing an embedded plugin
		pEmbeddedApp = NPL_EmbedCreate(pContext, pLayoutData);
		if(pEmbeddedApp == NULL)
			return;

		//  We're going to have to load.
        pItem = new CNetscapeCntrItem(this);
		if (fullPageOLE)
			pItem->m_isFullPage = TRUE;
        pItem->m_iLock++;

        //  Add this layout item to the number of layout items accessing us.
        pItem->m_cplElements.AddTail(pLayoutData);

        // Mark what actions to take with layout, depending on blocking or non blocking.
		// Note that hidden plugins never block layout and they don't require displaying
		//
		// If we're printing we will use a cached app from the session data so we don't
		// need to do either of these
		if ((pLayoutData->ele_attrmask & LO_ELE_HIDDEN) == 0 && !bPrinting) {
			if(pLayoutData->width == 0 || pLayoutData->height == 0) {
				//  Layout is blocking, be sure to unblock once loaded.
				pItem->m_cplUnblock.AddTail(pLayoutData);
			}
			else    {
				//  Layout isn't blocking, but we'll need to manually display this once it is loaded.
				pItem->m_cplDisplay.AddTail(pLayoutData);
			}
		}

		// If we are printing a plugin then NPL_EmbedCreate() just returns the
		// cached app from the session data
		if (bPrinting) {
			// Because we are using a cached app from the session data, save
			// the existing pItem so we can restore it when we free the embed item
			pItem->m_pOriginalItem = (CNetscapeCntrItem*)pEmbeddedApp->fe_data;

			ASSERT(pItem->m_pOriginalItem);

			if (pEmbeddedApp->type == NP_OLE && pItem->m_pOriginalItem && 
				pItem->m_pOriginalItem->m_bDelayed == FALSE &&
				pItem->m_pOriginalItem->m_bBroken == FALSE &&
				pItem->m_pOriginalItem->m_bLoading == FALSE) {
				// Have the OLE object load itself from the same temp file
				if (pItem->m_pOriginalItem) {
					pItem->m_csFileName = pItem->m_pOriginalItem->m_csFileName;                        
					pItem->CreateCloneFrom( pItem->m_pOriginalItem);
				}
                CSize  csExtents;
				CDCCX* pCX = VOID2CX(pContext->fe.cx, CDCCX);

                pItem->m_pOriginalItem->GetExtent(&csExtents);

				csExtents.cx = pCX->Metric2TwipsX(csExtents.cx);
				csExtents.cy = pCX->Metric2TwipsY(csExtents.cy);
				if ( pLayoutData->width )
					csExtents.cx = pLayoutData->width; 
				if ( pLayoutData->height )
					csExtents.cy = pLayoutData->height; 
				pLayoutData->width = csExtents.cx;
				pLayoutData->height = csExtents.cy;
                // In the printing case, an OLE container is not windowed
                LO_SetEmbedType(pLayoutData, PR_FALSE);
			}
			else { 
				// so layout will not block on us. Since this embed element is missing.
				pLayoutData->width = 1;
				pLayoutData->height = 1;

			}

		} else {
			if(pEmbeddedApp->wdata == NULL) {
				// Just created
				pEmbeddedApp->wdata = XP_NEW_ZAP(NPWindow);
				if(pEmbeddedApp->wdata == NULL)
					return;

			} else {
				// Check if there's a child window and if we're in a frame cell. Note:
				// there may not be a window, especially if the plugin is in a nested table
				if (pContext->is_grid_cell && pEmbeddedApp->wdata->window &&
                    (pEmbeddedApp->wdata->type == NPWindowTypeWindow)) {
					// Reparent the window onto the current window in case it got
					// moved from a frame cell (e.g. during a resize)
					::SetParent((HWND)pEmbeddedApp->wdata->window, WINCX(pContext)->GetView()->m_hWnd);
					::ShowWindow((HWND)pEmbeddedApp->wdata->window, SW_SHOW);

					// Adobe hack. Turn on clip children
					CGenericView *pGView =  WINCX(pContext)->GetView();
					if(pGView) {
					    CFrameGlue *pGlue = pGView->GetFrame();
					    if(pGlue) {
					        pGlue->ClipChildren(CWnd::FromHandle((HWND)pEmbeddedApp->wdata->window), TRUE);
					    }
					}
				}
			}

			pItem->m_bLoading = TRUE;
		}

		pEmbeddedApp->fe_data = (void *)pItem;
		pItem->m_csAddress = pLayoutAddress;

		//  Go ahead and assign it into the layout data for use in other functions.
		pLayoutData->FE_Data = (void *)pEmbeddedApp;

		// Now that we've set the NPEmbeddedApp's fe_data and layout's FE_Data we can start the embed
		if (NPL_EmbedStart(pContext, pLayoutData, pEmbeddedApp) != NPERR_NO_ERROR) {
			// Something went wrong. Time to clean up. The XP code has already deleted
			// the NPEmbeddedApp
			pLayoutData->FE_Data = NULL;
			pItem->m_bLoading = FALSE;
			delete pItem;
		}
    }

}

void CGenericDoc::FreeEmbedElement(MWContext *pContext, LO_EmbedStruct *pLayoutData)
{
    NPEmbeddedApp* 		pEmbeddedApp = (NPEmbeddedApp*)pLayoutData->FE_Data;
    CNetscapeCntrItem*	pItem = NULL;
    CNetscapeCntrItem*	curItem = NULL;
    int32 iRefCountIndicator = 0;


    // It's possible that pEmbeddedApp is NULL if the plugin failed to
    // initialize
	if (! pEmbeddedApp)
        return;

    // this thing is going to be decremented in NPL_EmbedDelete, so 
    iRefCountIndicator = NPL_GetEmbedReferenceCount(pEmbeddedApp);

    pItem = (CNetscapeCntrItem *)pEmbeddedApp->fe_data;

    if(iRefCountIndicator > 0)
        {
            // Restore the pItem if necessary. This is needed for printing of
            // embedded apps because we use the cached app from the session data
            // See comments in GetEmbedSize()
            if (pItem && pItem->m_pOriginalItem)
                pEmbeddedApp->fe_data = (void *)pItem->m_pOriginalItem;
        }

    // pEmbeddedApp will be freed in the call to NPL_EmbedDelete
    NPWindow *pAppWin = pEmbeddedApp->wdata;
    BOOL	  bFullPage = pEmbeddedApp->pagePluginType == NP_FullPage;

    // for OLE to clean up the CNetscapeCntrItem.
    if (pEmbeddedApp->type == NP_OLE)
        curItem = (CNetscapeCntrItem*)pEmbeddedApp->fe_data;

    NPL_EmbedDelete(pContext, pLayoutData);
    pLayoutData->FE_Data = NULL;

	//	If the item is already gone, bail now.
    // Also check for npdata->refs being greater or equal 0
    // It should not happen but
    // www.msn.com cashes us on resizing if this is not checked.

    // but first... Just in case since if pContext were 0 it would've crashed already
    if(pContext == NULL)
        iRefCountIndicator++;
    // because in this case NPL_EmbedDelete does not decrement it

    if ((pItem == NULL) || (iRefCountIndicator <= 0))
		return;

    //  Decrement the lock.
    pItem->m_iLock--;

    //  Remove this element from the list of items accessing us.
    pItem->m_cplElements.RemoveAt(pItem->m_cplElements.Find(pLayoutData));

    //  If the lock is zero'd, then we can free off the embed completely when it expires.
    //  We should probably try saving modified files here....  If I knew how.
    if(pItem->m_iLock == 0) {
        CString csRemoveMe = pItem->m_csFileName;

		//  If the app hasn't been deleted, break the reference
		//  from the app to the CNetscapeCntrItem.
		if (curItem == pItem) { 
			if (pEmbeddedApp != NULL)
				pEmbeddedApp->fe_data = NULL;
			if ( pItem->m_lpObject) {
				LPPERSISTSTORAGE lpPersistStorage =
					QUERYINTERFACE(pItem->m_lpObject, IPersistStorage);
				ASSERT(lpPersistStorage != NULL);
				// removed	pItem->IsDirty() when I figure out how to get lpPersistStorage->IsDirty()
				// to do the right thing.
				const char* tempPtr = pItem->m_csAddress;
				if (lpPersistStorage && (pItem->IsDirty())) { 
					lpPersistStorage->Release();

					CString	destFileName;
					if (pItem->m_bCanSavedByOLE) {
						CString buf;
						int ret;
						destFileName.Empty();
						if (!NET_IsLocalFileURL((char*)tempPtr)) {
							AfxFormatString1(buf, IDS_SAVE_HTTP_PROMPT, pItem->m_csAddress);
							ret = AfxMessageBox(buf, MB_YESNO | MB_ICONEXCLAMATION);
							if (ret == IDYES) {
								BOOL result = PromptForFileName(pItem, destFileName, destFileName, FALSE);
								if (!result) // user cancel from the getFileDialog
									ret = IDNO;
							}
						}
						else  {
							AfxFormatString1(buf, IDS_SAVE_DOC_PROMPT, pItem->m_csDosName);
							ret = AfxMessageBox(buf, MB_YESNO | MB_ICONEXCLAMATION);
							destFileName = pItem->m_csDosName;
						}
						
						switch (ret) {

                        case IDYES:	 {
#ifdef XP_WIN32
                            SCODE sc = S_OK;
#else
                            HRESULT sc = S_OK;
#endif
                            LPSTORAGE lpStorage = NULL;
                            BOOL result = FALSE;
                            while (!result) {
                                result =  DoSaveFile( pItem, destFileName);
                                if (!result) {
                                    CString buf;
                                    AfxFormatString1(buf, IDS_FILESAVEFAILED, destFileName);
                                    AfxMessageBox(buf, MB_OK | MB_ICONEXCLAMATION);
                                    if (!PromptForFileName(pItem, destFileName, destFileName, FALSE))
                                        break;
                                }
                            }
                            break;
                        }
                        case IDNO:
                            break;
						}
					}
				}
			}

            //  If we've a file to remove, do so now.
			if(csRemoveMe.IsEmpty() == FALSE) {
				TRY {
					CFile::Remove(csRemoveMe);
				}
				CATCH(CFileException, e)    {
					//  Well, we couldn't remove the file on our own.
					//      Some locking must still be in place.
					//  Add it to the list of files to remove on exit.
					FE_DeleteFileOnExit(csRemoveMe, (const char *)pLayoutData->embed_src);
				}
				END_CATCH
                    }
		}
        delete pItem;

    }
}


/////////////////////////////////////////////////////////////////////////////
// CGenericDoc commands

BOOL CGenericDoc::OnOpenDocument(const char *pszPathName)   {
    TRACE("CGenericDoc::OnOpenDocument(%s)\n", pszPathName);

    //  Check if we need to set our ability to serialize.
    //  Simply save or empty out the name if available.
    if(!m_bOpenDocumentFileSet) {
        m_bOpenDocumentFileSet = TRUE;
        if(pszPathName) {
            m_csOpenDocumentFile = pszPathName;
        }
    }
    
    //  Regardless, call the base.
    //  It ends up calling serialize and we handle correctly by
    //      setting the above serialize flag.
    BOOL bRetval = COleServerDoc::OnOpenDocument(pszPathName);

    //  We only handle loads on real file names passed in.
    //  This causes the actual load.
    if(pszPathName && m_pContext) {
        bRetval = m_pContext->OnOpenDocumentCX(pszPathName);
    }

    return(bRetval);
}

//	Don't allow the frame to close if we've been told not to let it close.
BOOL CGenericDoc::CanCloseFrame(CFrameWnd *pFrame)  {

	if (pFrame && pFrame->m_hWnd) {
		HWND	hPopup;

		// Check if the frame is displaying a dialog box. If so, don't quit
		// that window
		hPopup = ::GetLastActivePopup(pFrame->m_hWnd);

		if (hPopup != pFrame->m_hWnd) {
			// Bring the window to the top of the z-order so the user can get a
			// visual clue as to why we aren't closing
			::BringWindowToTop(hPopup);
			return FALSE;
		}

#ifdef EDITOR
        // Check for changes to edit document and prompt to save:
        CFrameGlue *pGlue = CFrameGlue::GetFrameGlue(pFrame);
        if(pGlue && pGlue->GetMainContext() && !pGlue->GetMainContext()->IsDestroyed() ){
		    MWContext *pMWContext = pGlue->GetMainContext()->GetContext();
            if( pMWContext ){
                // Don't close if something is in progess, like drag and drop
                // (Browser or Editor)

                // Stop any active Editor plugin
                if (EDT_IS_EDITOR(pMWContext) && !CheckAndCloseEditorPlugin(pMWContext))
                    return FALSE;

                //  do NOT return false when in waitingMode.
                //      pMWContext->waitingMode
                //  waitingMode is the mode we are in when a user can not click on
                //      another link, not for whatever bastardized purpose you have
                //      come up with.
                //  Blocking this on criteria in general is confusing as hell to
                //      the user, because when they select exit, and we're looking
                //      up a host, they can't exit.  Whey they say exit, we exit.
                //  End of story.

                    // Don't close if we are in the process of saving
                    //   a file or user cancels when prompted to save,
		            if( pMWContext && EDT_IS_EDITOR(pMWContext) &&
                      pMWContext->edit_saving_url ||
                      (LO_GetEDBuffer( ABSTRACTCX(pMWContext)->GetDocumentContext() ) &&
                       !FE_CheckAndSaveDocument(pMWContext)) ) {
			        return FALSE;
		        }
            }
	    }
#endif /* EDITOR */                    
	}


    //  Don't ask the base, it will ask the user to save a file.
    return(CanClose());
}

BOOL CGenericDoc::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) {
    //  Don't handle ID_FILE_CLOSE, let someone else deal with it.
    if(nID == ID_FILE_CLOSE)    {
        return(FALSE);
    }
	else if(nID == ID_EDIT_COPY)	{
		//	If we have an embedded item, we'll want to copy it.
		//	Otherwise, don't handle this message.
		//	By not returning false, we decide to handle it.
		if(m_pEmbeddedItem == NULL)	{
			//	Don't handle it.
			//	This let's the frame deal with it in whatever manner
			//		it needs to.
			return(FALSE);
		}
	}

    //  Otherwise, let the base class handle it.
    return(COleServerDoc::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo));
}

COleServerItem *CGenericDoc::OnGetEmbeddedItem()    {
    TRACE("CGenericDoc::OnGetEmbeddedItem() %p\n", this);

    //  Called by the framework only when needed to get a COleServerItem
    //      that is associated with the document.
    CNetscapeSrvrItem *pItem = new CNetscapeSrvrItem(this);
    ASSERT_VALID(pItem);
    return(pItem);  
}

void CGenericDoc::OnEditCopy() 
{
	//	See OnCmdMsg to see exactly when this is called (special cased).

	//	If there's an embedded item, copy it to the clipboard.
	//	Disable this functionality, and you disable OLE linked documents.
	//	Work around it.  Forcing me to implement a solution here is the wrong
	//		thing to do.
	CNetscapeSrvrItem *pItem = GetEmbeddedItem();
	if(pItem != NULL)	{
		pItem->CopyToClipboard(TRUE);
	}
}

void CGenericDoc::OnOleUpdate() 
{
	TRACE("Updating OLE Embedded Server.\n");
	//	Have the document update in whatever terms are normally
	//		handled in MFC.
	//	The call to update all items causes the redraw in the container.
	//	The OnUpdateDocument call causes serialization fo the current state.
	//	Also, call SetModifiedFlag to force the implementations to think
	//		that the document is dirty.

	UpdateAllItems(NULL);
	SetModifiedFlag();
	if(IsInPlaceActive() == FALSE)	{
		//	Only update the document if embedded, not in place.
		OnUpdateDocument();
	}
}

void CGenericDoc::OnUpdateOleUpdate(CCmdUI* pCmdUI) 
{
	//	Always leave it enabled, don't know exactly when to turn it off or
	//		how to detect.
	pCmdUI->Enable(TRUE);	
}

void CGenericDoc::OnCloseDocument()	{
	 // set the flag so we will not realize the Palette. when closing.

	//	Call the base.
	COleServerDoc::OnCloseDocument();
}


void CGenericDoc::OnDeactivateUI( BOOL bUndoable )
{

	CInPlaceFrame* pFrameWnd = (CInPlaceFrame*)m_pInPlaceFrame;

	COleServerDoc::OnDeactivateUI(bUndoable);

	if(pFrameWnd)
		pFrameWnd->ReparentControlBars();
}

void CGenericDoc::OnShowControlBars( CFrameWnd *pFrameWnd, BOOL bShow )
{

	CInPlaceFrame* pInPlace = (CInPlaceFrame*)m_pInPlaceFrame;

	if(pInPlace)
		pInPlace->ShowControlBars(pFrameWnd, bShow);


}

 

//	OLE server extension to allow document and server item
//		to minimally function without a context being
//		present.
void CGenericDoc::CacheEphemeralData()
{
	ASSERT(GetContext());
	ASSERT(GetContext()->IsDestroyed() == FALSE);

	//	Basically take any data that we'll need in order to serialize
	//		the document.
	//	That is only in the 2.0 timeline the address of the current
	//		history entry!  Whew....

	//	Do we have a history entry from which to create
	//		the address?
	History_entry *pHistoryEntry = SHIST_GetCurrent(&(GetContext()->GetContext()->hist));
	if(pHistoryEntry == NULL)	{
		//	Nothing to do really.
	}
	else	{
		//	Save anything.
		//	If in the future, we attempt to access the context, and
		//		are unable, we will fall back to this information.
		m_csEphemeralHistoryAddress = pHistoryEntry->address;
	}
}

//	Resolve between actual context and ephemeral data.
void CGenericDoc::GetContextHistoryAddress(CString &csRetval)	{
	if(GetContext() && GetContext()->IsDestroyed() == FALSE)	{
		History_entry *pHistoryEntry = SHIST_GetCurrent(&(GetContext()->GetContext()->hist));
		if(pHistoryEntry == NULL)	{
			csRetval.Empty();
		}
		else	{
			csRetval = pHistoryEntry->address;
		}
	}
	else	{
		//	Return whatever epemeral data we have.
		csRetval = m_csEphemeralHistoryAddress;
	}
}


//	Enable or disable closing of the frame window.
void CGenericDoc::EnableClose(BOOL bEnable)
{
	//	Just remember the value.
	m_bCanClose = bEnable;

	//	We want to recusively walk down all child contexts to
	//		set their values also.
	MWContext *pChild;
	XP_List *pTraverse = GetContext()->GetContext()->grid_children;
	while((pChild = (MWContext*)XP_ListNextObject(pTraverse)))	{
        if(ABSTRACTCX(pChild) && ABSTRACTCX(pChild)->IsDCContext() && VOID2CX(pChild->fe.cx, CDCCX)->GetDocument()) {
		    VOID2CX(pChild->fe.cx, CDCCX)->GetDocument()->EnableClose(bEnable);
        }
	}
}

BOOL CGenericDoc::CanClose()
{
	//	Assume our retval is initially what we have set.
	//	We want to recursively walk down all child contexts to
	//		see if they are not allowing close.
	BOOL bRetval = m_bCanClose;
    if(GetContext())    {
	    MWContext *pChild;
		XP_List *pTraverse = GetContext()->GetContext()->grid_children;
		while(bRetval && (pChild = (MWContext*)XP_ListNextObject(pTraverse))) {
            if(ABSTRACTCX(pChild) && ABSTRACTCX(pChild)->IsDCContext() && VOID2CX(pChild->fe.cx, CDCCX)->GetDocument()) {
		        bRetval = VOID2CX(pChild->fe.cx, CDCCX)->GetDocument()->CanClose();
            }
	    }
    }
    return(bRetval);
}

// This OnSaveDocument should take care of 3 types of saving, when this routine is
// changed should test on the followin.
// 1. saving an HTML file from the File|SaveAs menu.
// 2. Save a OLE container item, opening up a .doc file from the directory listing,
// after the page is loaded, double click on the item, start in-place edit, type in
// some changes, then do File |Save As.
// 3. Test as an OLE inplace server.  Opening up wordPad, do insert object with
// create from file.
BOOL CGenericDoc::OnSaveDocument( LPCTSTR lpszPathName )
{
	// Since this will only happen with File | Save as, so it is safe to do this.
	CWinCX* pwincx = (CWinCX*)m_pContext;
    CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)GetInPlaceActiveItem(CWnd::FromHandle(pwincx->GetPane()));
	if (pItem) { // we had an inplace edited item.
		CString fileName;
	
		if (lpszPathName == NULL) {// need to bring up the saveAs dialog box
							  // here.
			BOOL result = PromptForFileName(pItem, fileName, pItem->m_csAddress, FALSE);
			if (!result)
				return TRUE;  // user cancel here.
		}
		else {// using the default control item name.
			fileName = pItem->m_csDosName;
		}
		BOOL result = DoSaveFile(pItem, fileName);

		if (!result) {
			CString buf;
			AfxFormatString1(buf, IDS_FILESAVEFAILED, fileName);
			AfxMessageBox(buf, MB_OK | MB_ICONEXCLAMATION);
		}
		return TRUE;  // return true here, so the regular navigator save as dialog box 
					// will not shows up. This is to handle File | Save As ... 
					// when we are in inplace edited.
	}

	POSITION pos = GetStartPosition( );
	if (GetNextServerItem(pos)) // OLE contain as us to save the document.
		return COleServerDoc::OnSaveDocument(lpszPathName);
	else 
		return FALSE; // let navigator does the normal saveAs operation.
}


BOOL CGenericDoc::PromptForFileName(CNetscapeCntrItem* pItem, CString& lpszPathName, CString &orgFileName, BOOL useDefaultDocName)
{
	CString filter;
	CString ext;

	int dot_index = orgFileName.ReverseFind('.');
	if (dot_index > 0) {
	   ext = orgFileName.Right(3);
	}

	char name[255];
	pItem->GetUserType(USERCLASSTYPE_FULL, filter);

	OPENFILENAME fname;
	filter += '\0';
	filter += "*.";
	filter += ext;
	filter += '\0';
	CString s;
	s.LoadString(IDS_FILTER_ALLFILES);
	filter += s;
	filter += '\0';
	filter += "*.*";
	filter += '\0';
	filter += '\0';
	memset(&fname, 0, sizeof(fname));
	fname.lStructSize = sizeof(OPENFILENAME);
	fname.hwndOwner = pItem->GetActiveView()->GetSafeHwnd();
	fname.lpstrFilter = filter;
	fname.lpstrCustomFilter = NULL;
	fname.nFilterIndex = 1;
	char FullName[255];
	FullName[0] = 0;
	strcpy(&FullName[1], ext);
	fname.lpstrFile = FullName;
	fname.nMaxFile = _MAX_PATH;
	fname.lpstrFileTitle = name;
	fname.nMaxFileTitle = _MAX_FNAME;
	fname.lpstrInitialDir = NULL;
	CString title;
	CString temp;
	title.LoadString(IDS_SAVE_AS);
	pItem->GetUserType(USERCLASSTYPE_FULL, temp);
	title += temp;
	pItem->GetUserType(USERCLASSTYPE_FULL, title);
	fname.lpstrTitle = title;
	fname.lpstrDefExt = ext;
	fname.Flags = OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	BOOL bResult;
	bResult = FEU_GetSaveFileName(&fname);
	// prompt, filter, index, CWnd::FromHandle(m_hWnd));
	if (bResult) { // user choose OK
		lpszPathName = FullName;
	}
	return bResult;
}

// If there is an error, an should ask user for futher info to save to the file, then return FALSE,
// otherwise return TRUE.  meaning this file can not be saved at all.
BOOL CGenericDoc::DoSaveFile( CNetscapeCntrItem* pItem, LPCTSTR lpszPathName)
{
	COleDispatchDriver pDispDrv;
	int _convert;
	BOOL cont = TRUE;
	LPCOLESTR lpsz =T2COLE(lpszPathName); 
#ifdef XP_WIN32
	LPDISPATCH pdisp = QUERYINTERFACE(pItem->m_lpObject, IDispatch);
	WIN32_FIND_DATA fileHandle;

	HANDLE fHandle = FindFirstFile(lpszPathName, &fileHandle);
	// only do this for existing file.
	if ((fHandle != INVALID_HANDLE_VALUE) && pdisp && (pItem->m_idSavedAs != DISPID_UNKNOWN)){
		pDispDrv.AttachDispatch( pdisp, FALSE);
		if (pItem->m_idSavedAs) {
			VARIANT v;	
			TRY {
				static BYTE parms[] = VTS_VARIANT;
				V_VT(&v) = VT_BSTR;
				V_BSTR(&v) = SysAllocString(lpsz);
				pDispDrv.InvokeHelper(pItem->m_idSavedAs, DISPATCH_METHOD, VT_EMPTY, (void*)NULL, parms,
					&v);
				cont =  FALSE;
			}
			CATCH( COleException, e) {
				if (StgIsStorageFile(lpsz) != S_OK) {
					// this doc object does not support compound file,
					// should abort saving operation here.
					if (!pItem->ReportError(e->m_sc)) {
						AfxMessageBox(AFX_IDP_FAILED_TO_SAVE_DOC);
					}
					cont = FALSE;
				}
			}
			AND_CATCH( COleDispatchException, e) {
				if (StgIsStorageFile(lpsz) != S_OK) {
					// this doc object does not support compound file,
					// should abort saving operation here.
					// Display message box for dispatch exceptions.
					AfxMessageBox((LPCTSTR)e->m_strDescription, 
						MB_ICONEXCLAMATION | MB_OK);
					cont = FALSE;
				}
			}
			END_CATCH
		}
	}

	if (!cont) {
		pDispDrv.DetachDispatch();
		pdisp->Release();
		return TRUE;
	}
#endif
	HRESULT sc;
	lpsz =T2COLE(lpszPathName); 

	IStorage * pStg;

	sc = StgOpenStorage(lpsz, NULL,
		STGM_READWRITE|STGM_TRANSACTED|STGM_SHARE_EXCLUSIVE,
		0, 0, &pStg);
	if (sc != S_OK) {
		// convert existing storage file
		sc = StgCreateDocfile(lpsz, STGM_READWRITE|
			STGM_TRANSACTED|STGM_SHARE_EXCLUSIVE,
			0, &pStg);
	}

		 
	if  (sc == S_OK) { 

		LPPERSISTSTORAGE lpPersistStorage =
			QUERYINTERFACE(pItem->m_lpObject, IPersistStorage);
		ASSERT(lpPersistStorage != NULL);
		if (lpPersistStorage) {
			sc = lpPersistStorage->HandsOffStorage();
			if (sc == S_OK) {
				sc = ::OleSave(lpPersistStorage, pStg, FALSE);
				if (sc == S_OK) {
					lpPersistStorage->SaveCompleted(NULL);
					pStg->Commit(STGC_DEFAULT);
				}
				pStg->Release();
			}
			lpPersistStorage->Release();
			if (sc != S_OK) {
				// error handling here.
				return FALSE;
			}
			else return TRUE;
		}
	}
	return FALSE;
}

