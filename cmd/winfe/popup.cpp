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

#include "cntritem.h"
#include "cxsave.h"
#include "netsvw.h"
#include "mainfrm.h"
#include "shcut.h"
#include "shcutdlg.h"
#include "np.h"
#include "feembed.h"
#include "feimage.h"
#include "prefInfo.h"
#include "mailmisc.h"
#include "libi18n.h"

//
// If the mouse is over a link or an image, allow the user to copy the URL
// to the clipboard
//
BOOL CNetscapeView::AddClipboardToPopup(CMenu * pMenu, LO_Element * pElement, BOOL bAddSeparator)
{
	BOOL	bLink, bImage, bCopy;

	// See if it can be copied
	bCopy = GetContext()->CanCopySelection();

	//	See if it's a link
	CString csURL = GetAnchorHref(pElement);
	bLink = !csURL.IsEmpty();

	//	See if it's an image
	csURL = GetImageHref(pElement);
	bImage = !csURL.IsEmpty();

	if((bCopy || bLink || bImage) && bAddSeparator)
		pMenu->AppendMenu(MF_SEPARATOR);

	
	if(bCopy)
	{
		pMenu->AppendMenu(MF_ENABLED,
						  ID_EDIT_COPY,
						  szLoadString(ID_POPUP_COPY));
	}


	if (bLink) {
		pMenu->AppendMenu(MF_ENABLED,
						  ID_POPUP_COPYLINKCLIPBOARD,
						  szLoadString(IDS_POPUP_COPYLINKCLIPBOARD));
	}

	if (bImage) {
		pMenu->AppendMenu(MF_ENABLED,
						  ID_POPUP_COPYIMGLOC2CLIPBOARD,
						  szLoadString(IDS_POPUP_COPYIMGLOC2CLIPBOARD));
	}


	return(bLink || bCopy || bImage || bAddSeparator);
}

//
// If the mouse is over a link, add the appropriate items to the list
// If we're in a browser window then bBrowser is TRUE.
BOOL CNetscapeView::AddLinkToPopup(CMenu * pMenu, LO_Element * pElement, BOOL bBrowser)
{
	UINT uState, uSaveAsState, uMailtoState;

	//	If there is no link just return
	CString csAppend = m_csRBLink;
	BOOL bLink = !csAppend.IsEmpty();
	BOOL bFrame = FALSE;

	CString csEntry;

	if(bLink)	{
        // We need slightly different text in menu to differentiate between
        //  Browser and Editor windows
        // "Browse to: "
        csEntry.LoadString(IDS_POPUP_LOAD_LINK_EDT);
		uState = MF_ENABLED;

		LPSTR	lpszSuggested = 
#ifdef MOZ_MAIL_NEWS      
         MimeGuessURLContentName(GetContext()->GetContext(), csAppend);
#else
         NULL;
#endif                  
        if (lpszSuggested) {
            csEntry += "(";
            csEntry += lpszSuggested;
            csEntry += ")";
			XP_FREE(lpszSuggested);

        } else {
            WFE_CondenseURL(csAppend, 25);
	    	csEntry += csAppend;		
		}

		csAppend.Empty();


		//	Need to figure out mailto state, and any other URLs
		//		that won't make sense in a new window, or with
		//		save as.
		if(strnicmp(m_csRBLink, "mailto:", 7) == 0 || strnicmp(m_csRBLink, "telnet:", 7) == 0 ||
		    strnicmp(m_csRBLink, "tn3270:", 7) == 0 || strnicmp(m_csRBLink, "rlogin:", 7) == 0)	{
			uMailtoState = MF_GRAYED;
		}
		else	{
			uMailtoState = MF_ENABLED;
		}

        //  Need to figure out if we can do a save as without interrupting the window.
		//	But now, we don't need to!
        uSaveAsState = MF_ENABLED;
        if(uMailtoState == MF_GRAYED)   {
            uSaveAsState = MF_GRAYED;
        }
	}

    // Add "Open in Editor" only if a Browser context
    //  because of a bug (18428) in editing link in mail message
    // TODO: Lou fixed mail bug -- remove this context check later
    // "Open in &New Browser Window"

	CString cs;

	if(bLink)
	{
		if(bBrowser)
			cs.LoadString(IDS_POPUP_LOADLINKNEWWINDOW);
		else
			cs.LoadString(IDS_POPUP_LOADLINKNAVIGATOR);

		pMenu->AppendMenu(uMailtoState, ID_POPUP_LOADLINKNEWWINDOW, cs);
	}


	if(GetContext() && GetContext()->IsGridCell())
	{
		cs.LoadString(IDS_POPUP_LOADFRAMENEWWINDOW);
		bFrame = TRUE;
		pMenu->AppendMenu(MF_ENABLED, ID_POPUP_LOADFRAMENEWWINDOW, cs);
	}

#ifdef EDITOR
	if(bLink)
	{
        int type = NET_URL_Type(m_csRBLink);
        // Only add menu item if we are sure we can edit the link
        if( type == FTP_TYPE_URL  ||
            type == HTTP_TYPE_URL ||
            type == SECURE_HTTP_TYPE_URL ||
            type == FILE_TYPE_URL )
        {
    	    cs.LoadString(IDS_POPUP_EDIT_LINK);
            pMenu->AppendMenu(uMailtoState,ID_POPUP_EDIT_LINK, cs);
        }
	}
#endif /* EDITOR */

	return(bLink || bFrame);
}


//
// If the mouse is over an embedded object, add the appropriate items to the list
//
BOOL CNetscapeView::AddEmbedToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer, BOOL bAddSeparator)
{
	
    UINT uState, uInlineState, uSaveAsState;
    CString csAppend, csEntry;

	//	Inline
    CNetscapeCntrItem *pItem = NULL;
    if(pElement != NULL && pElement->type == LO_EMBED)  {
		NPEmbeddedApp *pPluginShim = (NPEmbeddedApp *)((LO_EmbedStruct *)pElement)->objTag.FE_Data;
		if(pPluginShim != NULL && wfe_IsTypePlugin(pPluginShim) == FALSE)	{
        	pItem = (CNetscapeCntrItem *)pPluginShim->fe_data;
		}
    }

    if(pItem != NULL && pItem->m_bLoading == FALSE && pItem->m_lpObject != NULL)  {
        //  Embed rules!

        csAppend = m_csRBEmbed;

        // if there is no embeded element don't do anything
        if(csAppend.IsEmpty())
            return bAddSeparator;

		if(bAddSeparator)
			pMenu->AppendMenu(MF_SEPARATOR);

        csEntry.LoadString(IDS_POPUP_LOADEMBED);

        if(csAppend.IsEmpty() == TRUE || pItem->m_bBroken == TRUE)  {
            uState = MF_GRAYED;
            uInlineState = MF_GRAYED;
            uSaveAsState = MF_GRAYED;
            csAppend.LoadString(IDS_POPUP_NOEMBED);
            csEntry += csAppend;
        }
        else    {
            CString csActivate;
            csActivate.LoadString(IDS_POPUP_ACTIVATE_EMBED);
            csEntry = csActivate + csEntry;
            uState = MF_ENABLED;
            WFE_CondenseURL(csAppend, 25);
            csEntry += csAppend;
            csAppend.Empty();

            //  Eventually, we'll need to do selective inline loading of embedded items.
            uInlineState = MF_GRAYED;

            //  Need to figure out if we can do a save as without interrupting the window.
            uSaveAsState = MF_ENABLED;
        }
        pMenu->AppendMenu(uState, ID_POPUP_ACTIVATEEMBED, csEntry);

        csEntry.LoadString(IDS_POPUP_SAVEEMBEDAS);
        pMenu->AppendMenu(uSaveAsState, ID_POPUP_SAVEEMBEDAS, csEntry);

        csEntry.LoadString(IDS_POPUP_COPYEMBEDLOC);
        pMenu->AppendMenu(MF_ENABLED, ID_POPUP_COPYEMBEDLOC, csEntry);

        CString csDescription;
        if(pItem->m_bBroken == TRUE)    {
			csDescription.LoadString(IDS_UNKNOWN_DOCTYPE);
        }
        else    {
            pItem->GetUserType(USERCLASSTYPE_SHORT, csDescription);
        }
        csEntry.LoadString(IDS_POPUP_COPYEMBEDDATA);
        csEntry += csDescription;
        pMenu->AppendMenu(uState, ID_POPUP_COPYEMBEDDATA, csEntry);

        //  Selective inline loading to be done here.

		return TRUE;
    }

	return bAddSeparator;
}

BOOL CNetscapeView::AddSaveItemsToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer, BOOL bAddSeparator)
{
	BOOL bLink, bImage;

	//	See if it's a link
	CString csURL = m_csRBLink;
	bLink = !csURL.IsEmpty();

	//	See if it's an image
	CString imageURL = m_csRBImage;
	bImage = !imageURL.IsEmpty();

	CString csEntry;

	if((bLink || bImage) && bAddSeparator)
		pMenu->AppendMenu(MF_SEPARATOR);

	if(bLink)
	    pMenu->AppendMenu(MF_ENABLED, ID_POPUP_SAVELINKCONTENTS,
		                  szLoadString(IDS_POPUP_SAVELINKCONTENTS));

	if(bImage && pElement && pElement->type == LO_IMAGE)
	{
		LO_ImageStruct *pImage = (LO_ImageStruct *)pElement;
#ifdef EDITOR
		// Save the image object just like we do
		//   during drag n drop so we can copy
		//   all image data to clipboard
		GetContext()->m_pLastImageObject = pImage;
#endif

		// Allow them to save the image, or use it as wallpaper
		//  Only, can't save internal icons.
		csEntry.LoadString(IDS_POPUP_SAVEIMAGEAS);
		IL_ImageReq *image_req = pImage->image_req;
		IL_Pixmap *image = IL_GetImagePixmap(image_req);
		if (image) {
			FEBitmapInfo* imageInfo = (FEBitmapInfo*)image->client_data;
			if (!image->bits && imageInfo && !imageInfo->hBitmap)
				pMenu->AppendMenu(MF_GRAYED,ID_POPUP_SAVEIMAGEAS, csEntry);
			else
				pMenu->AppendMenu( MF_ENABLED,ID_POPUP_SAVEIMAGEAS, csEntry);
		}

	} else {
        LO_ImageStruct *pLOImage;
        CL_Layer *parent_layer;
        char *layer_name;
        layer_name = CL_GetLayerName(layer);
        if (layer_name && ((XP_STRCMP(layer_name, LO_BODY_LAYER_NAME) == 0) ||
                    (XP_STRCMP(layer_name, LO_BACKGROUND_LAYER_NAME) == 0))) {
            parent_layer = CL_GetLayerParent(layer);
            pLOImage = LO_GetLayerBackdropImage(parent_layer);
        }
        else
            pLOImage = LO_GetLayerBackdropImage(layer);

        if (pLOImage && pLOImage->image_req) {
            CString	strEntry;
           
            m_csRBImage = (char *) pLOImage->image_url;
            strEntry.LoadString(IDS_POPUP_SAVEBACKGROUNDAS);
            pMenu->AppendMenu(MF_ENABLED, ID_POPUP_SAVEIMAGEAS, strEntry);
            m_pRBElement = (LO_Element*)pLOImage;
            PA_UNLOCK(pLOImage->image_url);
            // Do your stuff here.
        }
	}

	return (bLink || bImage || bAddSeparator);
  }

void CNetscapeView::AddBrowserItemsToPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer)
{
	CString imageURL = GetImageHref(pElement);
	Bool bImage = !imageURL.IsEmpty();

	CString csEntry;

	if(bImage && pElement && pElement->type == LO_IMAGE)
	{
		LO_ImageStruct *pImage = (LO_ImageStruct *)pElement;

		#ifdef EDITOR
		// Save the image object just like we do
		//   during drag n drop so we can copy
		//   all image data to clipboard
			GetContext()->m_pLastImageObject = pImage;
		#endif

		IL_ImageReq *image_req = pImage->image_req;
		IL_Pixmap *image = IL_GetImagePixmap(image_req);
		if (image) {
			csEntry.LoadString(IDS_POPUP_SETASWALLPAPER);
			pMenu->AppendMenu(GetContext()->CanWriteBitmapFile(pImage) ? MF_ENABLED : MF_GRAYED,
				ID_POPUP_SETASWALLPAPER, csEntry);
		}

	} else {
		// this is the case for saving backdrop image.
        LO_ImageStruct *pLOImage;
        CL_Layer *parent_layer;
        char *layer_name;
        layer_name = CL_GetLayerName(layer);
        if (layer_name && ((XP_STRCMP(layer_name, LO_BODY_LAYER_NAME) == 0) ||
                    (XP_STRCMP(layer_name, LO_BACKGROUND_LAYER_NAME) == 0))) {
            parent_layer = CL_GetLayerParent(layer);
            pLOImage = LO_GetLayerBackdropImage(parent_layer);
        }
        else
            pLOImage = LO_GetLayerBackdropImage(layer);
        if (pLOImage && pLOImage->image_req) {
            CString	strEntry;
            
            m_csRBImage = (char*)pLOImage->image_url;
            strEntry.LoadString(IDS_POPUP_SETASWALLPAPER);
            pMenu->AppendMenu(MF_ENABLED, ID_POPUP_SETASWALLPAPER, strEntry);
            m_pRBElement = (LO_Element*)pLOImage;
            PA_UNLOCK(pLOImage->image_url);
            // Do your stuff here.
        }		
	}

	MWContext *pContext = GetContext()->GetContext();

	csEntry.LoadString(IDS_POPUP_ADDLINK2BOOKMARKS);
	pMenu->AppendMenu(		        
		(SHIST_GetCurrent(&(pContext->hist)) != NULL) ? MF_ENABLED : MF_GRAYED,
		ID_POPUP_ADDLINK2BOOKMARKS, csEntry);

	// check for shell support and add shortcut item if available
	CInternetShortcut InternetShortcut;
	if (InternetShortcut.ShellSupport())
	{
		csEntry.LoadString(IDS_POPUP_SHORTCUT);
   		pMenu->AppendMenu(
		    (SHIST_GetCurrent(&(pContext->hist)) != NULL) ? MF_ENABLED : MF_GRAYED,
		    ID_POPUP_INTERNETSHORTCUT, csEntry );

	}

	pMenu->AppendMenu((SHIST_GetCurrent(&(pContext->hist)) != NULL) ? MF_ENABLED : MF_GRAYED,
					   ID_POPUP_MAILTO, szLoadString(IDS_POPUP_SENDPAGE));

}

void CNetscapeView::AddNavigateItemsToPopup(CMenu *pMenu, LO_Element * pElement, CL_Layer *layer)
{
		CString csEntry;

		// Back
		csEntry.LoadString(IDS_POPUP_BACK);
		pMenu->AppendMenu(MF_STRING, ID_NAVIGATE_BACK, csEntry);
	
		//	Forward
		csEntry.LoadString(IDS_POPUP_FORWARD);
		pMenu->AppendMenu(MF_STRING, ID_NAVIGATE_FORWARD, csEntry);

		if(GetContext()->IsGridCell())
		{
			csEntry.LoadString(IDS_POPUP_RELOADFRAME);
			pMenu->AppendMenu(MF_STRING, ID_NAVIGATE_RELOADCELL, csEntry);
		}
		else
		{
			csEntry.LoadString(IDS_POPUP_RELOAD);
			pMenu->AppendMenu(MF_STRING, ID_NAVIGATE_RELOAD, csEntry);
		}

		if(!prefInfo.m_bAutoLoadImages)
		{
			csEntry.LoadString(IDS_POPUP_SHOWIMAGE);
			pMenu->AppendMenu(MF_ENABLED, ID_POPUP_LOADIMAGEINLINE, csEntry);
		}

		csEntry.LoadString(IDS_POPUP_STOP);
		pMenu->AppendMenu(MF_STRING, ID_NAVIGATE_INTERRUPT, csEntry);

}

//
// If the mouse is over a frame, add the appropriate items to the list
//
void CNetscapeView::AddViewItemsToPopup(CMenu * pMenu, MWContext *pContex, LO_Element * pElement)
{
	if(GetContext()->IsGridCell())
	{
		pMenu->AppendMenu(MF_SEPARATOR);
	    pMenu->AppendMenu(		        
			MF_ENABLED,
			ID_VIEW_FRAME_SOURCE, szLoadString(IDS_VIEWFRAMESOURCE));
		pMenu->AppendMenu(MF_ENABLED, ID_VIEW_FRAME_INFO, szLoadString(IDS_VIEWFRAMEINFO));

	}
	else
	{
		pMenu->AppendMenu(MF_SEPARATOR);
	    pMenu->AppendMenu(		        
			MF_ENABLED,
			ID_FILE_VIEWSOURCE, szLoadString(IDS_VIEWSOURCE));
		pMenu->AppendMenu(MF_ENABLED, ID_FILE_DOCINFO, szLoadString(IDS_VIEWINFO));

	}
	CString csURL, csEntry;

    // Make sure we have an image to begin with
	csURL = m_csRBImage;
	if(!csURL.IsEmpty())
	{
		MWContextType contentType = GetContext()->GetContext()->type;

		if (contentType != MWContextDialog) {

			// Allow them to view the image by itself in a separate window
			csEntry.LoadString(IDS_POPUP_LOAD_IMAGE);
			csEntry += ' ';
	
			// Tack the name of the URL on to the end, but shorten it to something
			// reasonable first
			LPSTR	lpszSuggested = 
#ifdef MOZ_MAIL_NEWS         
            MimeGuessURLContentName(GetContext()->GetContext(), csURL);
#else
            NULL;
#endif /* MOZ_MAIL_NEWS */                       
			if (lpszSuggested) {
				csEntry += "(";
				csEntry += lpszSuggested;
				csEntry += ")";
				XP_FREE(lpszSuggested);

			} else {
				WFE_CondenseURL(csURL, 25);
				csEntry += csURL;
			}

			pMenu->AppendMenu(MF_ENABLED, ID_POPUP_LOADIMAGE, csEntry);
		}
	}
	pMenu->AppendMenu(MF_SEPARATOR);
}

//
// Add the entries to create the right mouse menu over a web
//   browser window
//
void CNetscapeView::CreateWebPopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer)
{
	//	Start adding relevant items to the menu.
	MWContext *pContext = GetContext()->GetContext();

    // If editor view-source window, don't show any popup menu.
    if (pContext->edit_view_source_hack)
        return;

    //  Don't do anything else if we're in kiosk mode.
    if(!theApp.m_bKioskMode)    {
        //
        // If we are over a link add the appropriate elements
        //    
        if(AddLinkToPopup(pMenu, pElement, TRUE))
			pMenu->AppendMenu(MF_SEPARATOR);

		AddNavigateItemsToPopup(pMenu, pElement, layer);

		AddViewItemsToPopup(pMenu, pContext, pElement);

		AddBrowserItemsToPopup(pMenu, pElement, layer);
		
		// Add save image and link items
		AddSaveItemsToPopup(pMenu, pElement, layer, TRUE);

        //
        // If we are over an embedded object add the appropriate elements
        //    
        AddEmbedToPopup(pMenu, pElement, layer, TRUE);

		// Allow the user to copy URLs to the clipboard if appropriate
		AddClipboardToPopup(pMenu, pElement, TRUE);
    }
}

//
// Add the entries to create the right mouse menu over a news message
//

class CTestCmdUI: public CCmdUI {
public:
	BOOL m_bEnabled;

	CTestCmdUI() { m_bEnabled = FALSE; }
	virtual void Enable(BOOL bOn = TRUE) { m_bEnabled = bOn; }
	virtual void SetCheck(int nCheck = 1) {}
	virtual void SetRadio(BOOL bOn = TRUE) {}
	virtual void SetText(LPCSTR lpszText) {}
};

void CNetscapeView::CreateMessagePopup(CMenu * pMenu, LO_Element * pElement, CL_Layer *layer)
{
	BOOL bAddSeparator = FALSE;
    //
    // If we are over a link add the appropriate elements to the list
    //
    bAddSeparator = AddLinkToPopup(pMenu, pElement, FALSE);


	// Add Save Items 
	bAddSeparator = AddSaveItemsToPopup(pMenu, pElement, layer, bAddSeparator);
    //
    // If we are over an embedded object add the appropriate elements to the list
    //
    bAddSeparator = AddEmbedToPopup(pMenu, pElement, layer, bAddSeparator);

	// Allow the user to copy URLs to the clipboard if appropriate
	bAddSeparator = AddClipboardToPopup(pMenu, pElement, bAddSeparator);

	if(bAddSeparator)
		pMenu->AppendMenu(MF_SEPARATOR);

	// Start adding relevant items to the menu.
	// Test to see if "Post Reply" is enabled
	// If so, we're in a news message

	CTestCmdUI cmdUI;
	cmdUI.m_nID = ID_MESSAGE_POSTREPLY;
	cmdUI.DoUpdate( GetParentFrame(), FALSE );
	BOOL bNews = cmdUI.m_bEnabled;

#ifdef MOZ_MAIL_NEWS
	WFE_MSGBuildMessagePopup( pMenu->m_hMenu, bNews );
#endif /* MOZ_MAIL_NEWS */
}

void CNetscapeView::OnRButtonDown(UINT uFlags, CPoint cpPoint)	{
//	Purpose:	Bring up the popup menu.
//	Arguments:	uFlags	What meta keys are currently pressed, ignored.
//				cpPoint	The point at which the mouse was clicked in relative to the upper left corner of the window.
//	Returns:	void
//	Comments:	Saves the point in a class member, so other handling can occur through messages generated in the popup.
//	Revision History:
//		01-20-94	rewritten GAB
//

	//	Call the base class to handle the click.
	CGenericView::OnRButtonDown(uFlags, cpPoint);

	MWContext *pContext = GetContext()->GetContext();

	if (!pContext->compositor) 
	    OnRButtonDownForLayer(uFlags, cpPoint, 
				  (long)cpPoint.x, (long)cpPoint.y,
				  NULL);
}

BOOL CNetscapeView::OnRButtonDownForLayer(UINT uFlags, CPoint& cpPoint, 
					  long lX, long lY, CL_Layer *layer)	{
    LO_Element *pElement;
    HDC pDC = GetContextDC();
    MWContext *pContext = GetContext()->GetContext();

    if(pDC) 
      pElement = LO_XYToElement(ABSTRACTCX(pContext)->GetDocumentContext(), lX, lY, layer);
    else
      pElement = NULL;

    m_pLayer = layer;
	
	//	Save the point of the click.
	m_ptRBDown = cpPoint;
	m_csRBLink = GetAnchorHref(pElement);
	m_csRBImage = GetImageHref(pElement);
    m_csRBEmbed = GetEmbedHref(pElement);
	m_pRBElement = pElement;
	
	//	Create the popup.
	CMenu cmPopup;
	if(cmPopup.CreatePopupMenu() == 0)	{
	    return FALSE;
	}
	
    switch(GetContext()->GetContext()->type) {
    case MWContextMailMsg:
    case MWContextMail:
	// These two shouldn't occur
    case MWContextNewsMsg:
    case MWContextNews:
        CreateMessagePopup(&cmPopup, pElement, layer);
        break;
    default:
        // just fall through and show the browser popup even though it
        //  will probably be useless
    case MWContextBrowser:
        CreateWebPopup(&cmPopup, pElement, layer);
        break;
    }
	
	//	Track the popup now.
	ClientToScreen(&cpPoint);

    //  Certain types of contexts should not do this.
    if(
        GetFrame() &&
        GetFrame()->GetMainContext() &&
        GetFrame()->GetMainContext()->GetContext()->type != MWContextDialog) {
	    cmPopup.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpPoint.x, cpPoint.y, 
						       GetParentFrame(), NULL);
    }

    return TRUE;
}

void CNetscapeView::GetLogicalPoint(CPoint cpPoint, long *pLX, long *pLY)
{
    //	Get the device context, and figure out the logical point.
    HDC pDC = GetContextDC();
    if(pDC)	{
	CPoint cpLogical = cpPoint;
	::DPtoLP(pDC, &cpLogical, 1);
	
	//	Adjust the point further in context.
	//	Go to longs, as ints will wrap since we're in twips.
	MWContext *pContext = GetContext()->GetContext();
	*pLX = (long)cpLogical.x + (long)WINCX(pContext)->GetOriginX();
	*pLY = (long)cpLogical.y + (long)WINCX(pContext)->GetOriginY();
    }
    else {
	*pLX = (long)cpPoint.x;
	*pLY = (long)cpPoint.y;
    }
}

LO_Element *CNetscapeView::GetLayoutElement(CPoint cpPoint, CL_Layer *layer)	{
//	Purpose:	Return the layout element under the device point.
//	Arguments:	cpPoint The device point.
//	Returns:	LO_Element *	The layout element under the point, or NULL if over nothing.
//	Comments:	General utility function.
//	Revision History:
//		01-20-95	created GAB
//

    //	Get the device context, and figure out the logical point.
    HDC pDC = GetContextDC();
    if(pDC)	{
	long lX, lY;
	MWContext *pContext = GetContext()->GetContext();

	XP_Rect bbox;

	GetLogicalPoint(cpPoint, &lX, &lY);
	CL_GetLayerBbox(layer, &bbox);

	lX -= bbox.left;
	lY -= bbox.top;

	//	Return the element that layout reports at this coordinate.
		return(LO_XYToElement(ABSTRACTCX(pContext)->GetDocumentContext(), lX, lY, layer));
	}
	else	{
		return(NULL);
	}
}

CString CNetscapeView::GetAnchorHref(LO_Element *pElement)	{
//	Purpose:	Obtain the HREF to a layout element.
//	Arguments:	pElement	The abstract layout element, can be NULL.
//	Returns:	CString	The anchor, in full.
//	Comments:	Should handle other anchor cases as they become evident.
//	Revision History:
//		01-20-95	created
//

	CString csRetval;
	
	if(pElement != NULL)	{
		switch(pElement->type)	{
			case LO_TEXT:	{
				LO_TextStruct *pText = (LO_TextStruct *)pElement;
				if(pText->anchor_href && pText->anchor_href->anchor)	{
					csRetval = (char *)pText->anchor_href->anchor;
				}
				break;
			}
			case LO_IMAGE:	{
				LO_ImageStruct *pImage = (LO_ImageStruct *)pElement;
				if(pImage->image_attr->usemap_name != NULL)	{
					CPoint cpUsemap = m_ptRBDown;
					MWContext *pContext = GetContext()->GetContext();
					HDC pDC = GetContextDC();
					::DPtoLP(pDC, &cpUsemap, 1);
					cpUsemap.x -= (int)(pImage->x + pImage->x_offset + pImage->border_width - WINCX(pContext)->GetOriginX());
					cpUsemap.y -= (int)(pImage->y + pImage->y_offset + pImage->border_width - WINCX(pContext)->GetOriginY());
					::LPtoDP(pDC, &cpUsemap, 1);

					LO_AnchorData *pAnchorData = LO_MapXYToAreaAnchor(ABSTRACTCX(pContext)->GetDocumentContext(),
						pImage, cpUsemap.x, cpUsemap.y);

					if(pAnchorData != NULL && pAnchorData->anchor != NULL)	{
						csRetval = (char *)pAnchorData->anchor;
					}
				}
				else if(pImage->anchor_href && pImage->anchor_href->anchor)	{
					csRetval = (char *)pImage->anchor_href->anchor;
				
					//	Handle ismaps
					if(pImage->image_attr->attrmask & LO_ATTR_ISMAP)	{
						MWContext *pContext = GetContext()->GetContext();
						CPoint cpIsmap = m_ptRBDown;
						HDC pDC = GetContextDC();
						char aNumBuf[16];
					
						::DPtoLP(pDC, &cpIsmap, 1);
						cpIsmap.x -= (int)(pImage->x + pImage->x_offset + pImage->border_width - WINCX(pContext)->GetOriginX());
						cpIsmap.y -= (int)(pImage->y + pImage->y_offset + pImage->border_width - WINCX(pContext)->GetOriginY());
						::LPtoDP(pDC, &cpIsmap, 1);
					
						csRetval += '?';
						_itoa(cpIsmap.x, aNumBuf, 10);
						csRetval += aNumBuf;
						csRetval += ',';
						_itoa(cpIsmap.y, aNumBuf, 10);
						csRetval += aNumBuf;
					}
				}
				break;
			}
		}
	}
	
	return(csRetval);
}

CString CNetscapeView::GetImageHref(LO_Element *pElement)	{
//	Purpose:	Obtain the HREF to a layout element.
//	Arguments:	pElement	The abstract layout element, can be NULL.
//	Returns:	CString	The image anchor, in full.
//	Comments:
//	Revision History:
//		01-20-95	created
//

	CString csRetval;
	
	if(pElement != NULL)	{
		switch(pElement->type)	{
			case LO_IMAGE:	{
				LO_ImageStruct *pImage = (LO_ImageStruct *)pElement;
				if(pImage->image_url != NULL)	{
					//	Check for that funky internal-external-reconnect jazz.
					char *Weird = "internal-external-reconnect:";
					int iWeirdLen = strlen(Weird);
					if(strncmp((char *)pImage->image_url, Weird, iWeirdLen) == 0)	{
						// Use everything after "internal-external-reconnect:"
						csRetval = ((char *) pImage->image_url) + iWeirdLen;
					}
					else	{
						csRetval = (char *)pImage->image_url;
					}
				}
				break;
			}
		}
	}
	
	return(csRetval);
}

CString CNetscapeView::GetEmbedHref(LO_Element *pElement)	{
//	Purpose:	Obtain the HREF to a layout element.
//	Arguments:	pElement	The abstract layout element, can be NULL.
//	Returns:	CString	The embed anchor, in full.
//	Comments:
//	Revision History:
//		03-05-95	created
//

	CString csRetval;
	
	if(pElement != NULL)	{
		switch(pElement->type)	{
			case LO_EMBED:	{
				LO_EmbedStruct *pEmbed = (LO_EmbedStruct *)pElement;
				if(pEmbed->embed_src != NULL)	{
					csRetval = (char *)pEmbed->embed_src;
				}
				break;
			}
		}
	}
	
	return(csRetval);
}

void WFE_CondenseURL(CString& csURL, UINT uLength, BOOL bParens)	{
//	Purpose:	Condense a URL to a given length.
//	Arguments:	csURL	The URL to condense.
//				uLength	The maximum length.
//				bParens	Wether or not use parenthesis to surround the condesed form.
//						Also specifies wether or not we need to only extract the file name.
//	Returns:	void
//	Comments:
//	Revision History:
//		01-20-95	created GAB
//		01-28-95	Modified to only take the filename off, previously left protocol and right of the URL.
//		09-24-95	Well, now we're going back to cutting out the middle....
//					Parens code will only get the filename ending.
//

	char *pSlash;
	if(bParens == FALSE)	{
		pSlash = (char *)(const char *)csURL;
	}
	else	{
		//	Make it two shorter right now for the ().
		uLength -= 2;

		//	Find out if we have a forward slash in the URL, backwards.
		pSlash = strrchr(csURL, '/');
		if(pSlash != NULL)	{
			//	Make sure that this isn't just one character long (directory).
			if(strlen(pSlash) == 1)	{
				//	this is a directory, attempt to use the whole URL, I guess.
				pSlash = (char *)(const char *)csURL;
			}
		}

		if(pSlash == NULL)	{
			//	If there's no slash, attempt a : from the start (don't want to only show a port #!)
			pSlash = strchr(csURL, ':');
		}

		if(pSlash != NULL)	{
			//	Go one past what we've found, only if we're not at the start of the URL.
			if(pSlash != (char *)(const char *)csURL)	{
				pSlash++;
			}
		}
		else	{
			pSlash = (char *)(const char *)csURL;
		}
	}

	//	See if what we've found is longer than what we need.
	if(strlen(pSlash) <= uLength)	{
		//	No need to modify, just return what we have.
		CString csTemp;
		if(bParens == TRUE)	{
			csTemp += "(";
		}
		csTemp += pSlash;
		if(bParens == TRUE)	{
			csTemp += ")";
		}
		csURL = csTemp;
		return;
	}

	// Currently, we cannot do multilingual Menu, so use just treat all the menu
	// is in the resource csid.
	int16 guicsid = INTL_CharSetNameToID(INTL_ResourceCharSet());
	char truncated[120];
	if(uLength > 120)
		uLength = 120;

	INTL_MidTruncateString(guicsid, pSlash, truncated, uLength);
	
	if(bParens)	
	{
		csURL = "(";
		csURL += truncated;
		csURL += ")";
	}
	else
	{
		csURL = truncated;
	}
	
}

void CNetscapeView::OnPopupLoadLink()	{
//	Purpose:	Load the link
//	Arguments:	void
//	Returns:	void
//	Comments:	Never get's called unless actually over a link.
//	Revision History:
//		01-21-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//      13-Feb-95   modified to just call builtin load function - chouck
//
    MWContext * context = GetContext()->GetContext();
    History_entry *he = SHIST_GetCurrent (&context->hist);
	m_pContext->NormalGetUrl(m_csRBLink, (he ? he->address : NULL));
}

void CNetscapeView::OnPopupLoadLinkNewWindow()	{
//	Purpose:	Load a link into a new window.
//	Arguments:	void
//	Returns:	void
//	Comments:	Never gets called, unless actually over an image.
//				Need to handle image maps also.
//	Revision History:
//		01-21-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//      05-Jun-95   send the referred field too - chouck
//
    MWContext * context = GetContext()->GetContext();
    History_entry *he = SHIST_GetCurrent(&context->hist);

	//	Create the URL to load, assign a referrer field.
	URL_Struct *pUrl = NET_CreateURLStruct(m_csRBLink, NET_DONT_RELOAD);
	if(he->address != NULL)	{
		pUrl->referer = XP_STRDUP(he->address);
	}

	//	Always set the window_target for a new window.
	pUrl->window_target = XP_STRDUP("");

#ifdef EDITOR
    // Question: Does it matter if referer and window_target are set
    //           if we are creating a new context?
    if ( EDT_IS_EDITOR(context) ) {
        if ( pUrl ) {
            // Must clear this to correctly load URL into new context
            pUrl->fe_data = NULL;
            // Create new Context
            CFE_CreateNewDocWindow(NULL, pUrl);
        }
    } else
#endif
	CFE_CreateNewDocWindow(GetContext()->GetContext(), pUrl);
}

void CNetscapeView::OnPopupLoadFrameNewWindow()
{
	MWContext *context = GetContext()->GetContext();

    History_entry *he = SHIST_GetCurrent(&context->hist);

	URL_Struct *pUrl;
	//	Create the URL to load, assign a referrer field.
	if(he->address != NULL)
	{
		pUrl = NET_CreateURLStruct(he->address, NET_NORMAL_RELOAD);
	}

	//	Always set the window_target for a new window.
	pUrl->window_target = XP_STRDUP("");

#ifdef EDITOR
    // Question: Does it matter if referer and window_target are set
    //           if we are creating a new context?
    if ( EDT_IS_EDITOR(context) ) {
        if ( pUrl ) {
            // Must clear this to correctly load URL into new context
            pUrl->fe_data = NULL;
            // Create new Context
            CFE_CreateNewDocWindow(NULL, pUrl);
        }
    } else
#endif
	if(pUrl)
		CFE_CreateNewDocWindow(GetContext()->GetContext(), pUrl);

}

#ifdef EDITOR
// Load link location in a Composer window
void CNetscapeView::OnPopupLoadLinkInEditor()
{
    if ( GetContext()->GetContext() && !m_csRBLink.IsEmpty() ) {
        FE_LoadUrl((char*)LPCSTR(m_csRBLink), LOAD_URL_COMPOSER);
    }
}

// Load image in user-specified editor
void CNetscapeView::OnPopupEditImage()
{
    if ( GetContext()->GetContext() && !m_csRBImage.IsEmpty() ) {
        EditImage((char*)LPCSTR(m_csRBImage));
    }
}
#endif

void CNetscapeView::OnPopupSaveLinkContentsAs()	{
//	Purpose:	Save a link.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//		01-22-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//

	CSaveCX::SaveAnchorObject(m_csRBLink, NULL);
}

void CNetscapeView::OnPopupCopyLinkToClipboard()	{
//	Purpose:	Copy a link to the clipboard.
//	Arguments:	void
//	Returns:	void
//	Comments:	Don't free the handle.
//	Revision History:
//		01-22-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//

	CopyLinkToClipboard(m_hWnd, m_csRBLink);
}

void CNetscapeView::CopyLinkToClipboard(HWND hWnd, CString url)
{
	CString csHref = url;
	HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, csHref.GetLength() + 1);
	char *pCopy = (char *)GlobalLock(hData);	
	strcpy(pCopy, csHref);
	GlobalUnlock(hData);

    // Also copy bookmark-formatted data so we can paste full link,
    //  not just text, into the Editor or bookmark window
	::OpenClipboard(hWnd);
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hData);

    hData = GlobalAlloc(GMEM_DDESHARE | GMEM_ZEROINIT, sizeof(BOOKMARKITEM));
    if(hData){
        LPBOOKMARKITEM pBookmark = (LPBOOKMARKITEM)GlobalLock(hData);
        PR_snprintf(pBookmark->szAnchor, sizeof(pBookmark->szAnchor), "%s",
                    LPCSTR(csHref));
        PR_snprintf(pBookmark->szText, sizeof(pBookmark->szText), "%s",
                    LPCSTR(csHref));
        GlobalUnlock(hData);
    	::SetClipboardData(RegisterClipboardFormat(NETSCAPE_BOOKMARK_FORMAT), hData);
    }
	::CloseClipboard();
}

void CNetscapeView::OnPopupLoadImage()	{
//	Purpose:	Load an image inline that was delayed.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//		01-22-95	created GAB
//      13-Feb-95   modified to just call builtin load function - chouck

	if(GetContext())	{
	    MWContext * context = GetContext()->GetContext();
	    History_entry *he = SHIST_GetCurrent (&context->hist);

		//	Go up to the parent context.
		CAbstractCX *pCX = ABSTRACTCX(context);
		while(pCX && pCX->IsGridCell())	{
			pCX = ABSTRACTCX(pCX->GetParentContext());
		}

		if(pCX && pCX->IsFrameContext())	{
			pCX->NormalGetUrl(m_csRBImage, (he ? he->address : NULL));
		}
	}
}

void CNetscapeView::OnPopupLoadImageInline()	{
//	Purpose:	Load an image inline that was delayed.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//		01-22-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//
    const char *pURL = m_csRBImage;
    
    // Tell layout which image is to be force loaded.
	LO_SetForceLoadImage((char*)pURL, FALSE);

	m_pContext->ExplicitlyLoadThisImage(m_csRBImage);
}

void CNetscapeView::OnPopupSaveImageAs()	{
//	Purpose:	Save the image we're over to a file.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//		01-22-94	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//

	CSaveCX::SaveAnchorObject(m_csRBImage, NULL);
}

void CNetscapeView::OnPopupSetAsWallpaper()
{
	ASSERT(m_pRBElement && m_pRBElement->type == LO_IMAGE);
	if (m_pRBElement->type == LO_IMAGE) {
		char	szFileName[_MAX_PATH + 25];

		// We need to create a .BMP file in the Windows directory
		GetWindowsDirectory(szFileName, sizeof(szFileName));
#ifdef XP_WIN32
		strcat(szFileName, "\\Netscape Wallpaper.bmp");
#else
		strcat(szFileName, "\\Netscape.bmp");
#endif

		// Go ahead and write out the file
		if (GetContext()->WriteBitmapFile(szFileName, &m_pRBElement->lo_image)) {
			// Now ask Windows to change the wallpaper
			SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, szFileName, SPIF_UPDATEINIFILE);
		}
		
	}
}

void CNetscapeView::OnPopupCopyImageLocationToClipboard()	{
//	Purpose:	Copy the image's URL to the clipboard.
//	Arguments:	void
//	Returns:	void
//	Comments:	Don't free the handle.
//	Revision History:
//		01-22-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//

	CString csHref = m_csRBImage;
	HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, csHref.GetLength() + 1);
	char *pCopy = (char *)GlobalLock(hData);	
	strcpy(pCopy, csHref);
	GlobalUnlock(hData);
#ifdef EDITOR
    // Also copy all data for pasting into Editor - just like drag and drop
    HGLOBAL hImageData = NULL;
    if ( GetContext() ) {
        hImageData = WFE_CreateCopyImageData(GetContext()->GetContext(),
                                            GetContext()->m_pLastImageObject);
    }
    GetContext()->m_pLastImageObject = NULL;
#endif	

	OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hData);
#ifdef EDITOR
    if (hImageData ) {
        ::SetClipboardData(RegisterClipboardFormat(NETSCAPE_IMAGE_FORMAT), hImageData);
    }
#endif
	::CloseClipboard();
}

void CNetscapeView::OnPopupActivateEmbed()	{
//	Purpose:	Activate an embedded item.
//	Arguments:	void
//	Returns:	void
//	Comments:   Just about the same code as OnLButtonDblClk
//	Revision History:
//      03-06-95    created GAB

    CPoint cpPoint = m_ptRBDown;

    //  See if we are already selected in this area.
    //  If not, then we need to deselect any currently selected item.
    CWinCX *pWinCX = GetContext();
    if(pWinCX && pWinCX->m_pSelected)   {
        //  Figure out the rect that the item covers.
        long lLeft = pWinCX->m_pSelected->objTag.x + pWinCX->m_pSelected->objTag.y_offset - pWinCX->GetOriginX();
        long lRight = lLeft + pWinCX->m_pSelected->objTag.width;

        long lTop = pWinCX->m_pSelected->objTag.y + pWinCX->m_pSelected->objTag.y_offset - pWinCX->GetOriginY();
        long lBottom = lTop + pWinCX->m_pSelected->objTag.height;

        HDC pDC = pWinCX->GetContextDC();
        RECT crBounds;
		::SetRect(&crBounds, CASTINT(lLeft), CASTINT(lRight), CASTINT(lTop), CASTINT(lBottom));
        ::LPtoDP(pDC, (POINT*)&crBounds, 2);

        if(FALSE == ::PtInRect(&crBounds, cpPoint))  {
			OnDeactivateEmbed();
        }
        else    {
            //  Same item.  Don't do anything.
            return;
        }
    }

    if(m_pRBElement != NULL)    {
        if(m_pRBElement->type == LO_EMBED)  {
            LO_EmbedStruct *pLayoutData = (LO_EmbedStruct *)m_pRBElement;
			NPEmbeddedApp *pPluginShim = (NPEmbeddedApp *)pLayoutData->objTag.FE_Data;
			if(pPluginShim != NULL && wfe_IsTypePlugin(pPluginShim) == FALSE)	{
	            CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pPluginShim->fe_data;

	            //  Make sure we can load this.
	            if(pItem->m_bBroken == FALSE && pItem->m_bDelayed == FALSE && pItem->m_bLoading == FALSE)  {
	                //  We have an active selection.
	                pWinCX->m_pSelected = pLayoutData;

	                long lVerb = OLEIVERB_PRIMARY;
	                BeginWaitCursor();
	                pItem->Activate(lVerb, this);
	                EndWaitCursor();

	                //  If it's not in place active, then we don't care about it, as it's UI is being
	                //      handled elsewhere.
	                if(pItem->IsInPlaceActive() == FALSE)   {
	                    pWinCX->m_pSelected = NULL;
	                }
	            }
			}
        }
    }
}

void CNetscapeView::OnPopupSaveEmbedAs()	{
//	Purpose:	Save the embed we're over to a file.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//		01-22-94	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//      02-06-95    modified to handle embeds.
//
	CSaveCX::SaveAnchorObject(m_csRBEmbed, NULL);
}

void CNetscapeView::OnPopupCopyEmbedLocationToClipboard()	{
//	Purpose:	Copy the embed's URL to the clipboard.
//	Arguments:	void
//	Returns:	void
//	Comments:	Don't free the handle.
//	Revision History:
//		01-22-95	created GAB
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//      02-06-95    modified to handle embeds
//

	CString csHref = m_csRBEmbed;
	HANDLE hData = GlobalAlloc(GMEM_MOVEABLE, csHref.GetLength() + 1);
	char *pCopy = (char *)GlobalLock(hData);	
	strcpy(pCopy, csHref);
	GlobalUnlock(hData);
	
	OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_TEXT, hData);
	::CloseClipboard();
}

void CNetscapeView::OnPopupCopyEmbedToClipboard()   {
//	Purpose:	Copy the embed's data to the clipboard.
//	Arguments:	void
//	Returns:	void
//	Comments:
//	Revision History:
//      02-06-95    modified to handle embeds, created GAB
//

    if(m_pRBElement != NULL)    {
        if(m_pRBElement->type == LO_EMBED)  {
            LO_EmbedStruct *pLayoutData = (LO_EmbedStruct *)m_pRBElement;
			NPEmbeddedApp *pPluginShim = (NPEmbeddedApp *)pLayoutData->objTag.FE_Data;
			if(pPluginShim != NULL && wfe_IsTypePlugin(pPluginShim) == FALSE)	{
	            CNetscapeCntrItem *pItem = (CNetscapeCntrItem *)pPluginShim->fe_data;

	            //  Make sure we can copy this.
	            if(pItem->m_bBroken == FALSE && pItem->m_bDelayed == FALSE && pItem->m_bLoading == FALSE)  {
	                //  Do it.
	                pItem->CopyToClipboard(FALSE);
	            }
			}
        }
    }
}

void CNetscapeView::CreateTextAndAnchor(CString &csText, CString &csAnchor)
{
// Purpose: Merge common code from OnPopupAddLinkToBookmarks and OnPopupInternetShortCut
//
//
//	Revision History:
//		02-11-96	created
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//

    // See if we are over some text that is a link
    LO_TextStruct * text_struct = (LO_TextStruct *) m_pRBElement;
    LO_ImageStruct * image_struct = (LO_ImageStruct *) m_pRBElement;

	CString csAppend = m_csRBLink;

	if(text_struct && text_struct->type == LO_TEXT && text_struct->anchor_href) 
    {
        csText = (char *)text_struct->text;        
        csAnchor = m_csRBLink;
    } 
    else if(image_struct && image_struct->type == LO_IMAGE && image_struct->anchor_href) 
    {
        WFE_CondenseURL(csAppend,25);
        csText = csAppend;
        csAnchor = m_csRBLink;
    }
    else
    {
		MWContext *context = GetContext()->GetContext();

        History_entry *h = SHIST_GetCurrent (&context->hist);
        URL_Struct *url = SHIST_CreateURLStructFromHistoryEntry (context, h);
        
        if ((h != NULL) && (url != NULL))
        {
            csText = h->title;
            csAnchor = url->address;
        }  /* end if */
    }

    if (!csText.GetLength() )
    {
        csText = csAnchor;
        WFE_CondenseURL(csText,25);
    }
}

void CNetscapeView::OnPopupAddLinkToBookmarks()	
{
//	Purpose:	Add the link we're under to the bookmarks.
//	Arguments:	void
//	Returns:	void
//	Comments:	Never gets called, unless actually over a link.
//              The bookmark is not going in the correct place
//	Revision History:
//		01-21-95	created
//		01-24-95	modified to not query layout again for the element, in case a load occured in between.
//
	CString csText, csAnchor;
	CreateTextAndAnchor(csText, csAnchor);

    if (csAnchor.GetLength()) {
		HT_AddBookmark((char*)(const char*)csAnchor, (char*)(const char*)csText);
	}
}

void CNetscapeView::OnPopupInternetShortcut() {
	CString csText, csAnchor;
	CreateTextAndAnchor(csText, csAnchor);

   	if (csAnchor.GetLength()) {
        const char * szText = (const char *) csText;
        const char * szAnchor = (const char *) csAnchor;
		CShortcutDialog ShortcutDialog ( this, (char *)szText, (char *)szAnchor );
		if ( ShortcutDialog.DoModal ( ) == IDOK )
		{
			CInternetShortcut InternetShortcut ( 
				ShortcutDialog.GetDescription ( ), 
				(char *)szAnchor );
		}	
	}
}

void CNetscapeView::OnPopupMailTo() {
	// Pass this off to the context for handling.
	if(GetContext())        {
		GetContext()->MailDocument();
	}
}
