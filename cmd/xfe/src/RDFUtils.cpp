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

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// Name:        RDFUtils.cpp                                            //
//                                                                      //
// Description:	Misc RDF XFE specific utilities.                        //
//                                                                      //
// Author:		Ramiro Estrugo <ramiro@netscape.com>                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#include "RDFUtils.h"

#include "IconGroup.h"
#include "BrowserFrame.h"   // For fe_reuseBrowser()

#include "xp_str.h"
#include "xpassert.h"

#include "felocale.h"		// For fe_ConvertToXmString()
#include "intl_csi.h"		// For INTL_ functions
#include "prefapi.h"		// For PREF_GetIntPref

#include "xfe.h"			// For fe_FormatDocTitle()

#include <Xfe/Xfe.h>		// For XfeIsAlive()
#include <Xfe/Label.h>		// For XfeIsLabel()
#include <Xm/Label.h>		// For XmIsLabel()

#include <Xfe/Button.h>		// For XmBUTTON_ defines

#include <Xfe/BmButton.h>	// For XfeIsBmButton()
#include <Xfe/BmCascade.h>	// For XfeIsBmCascade()


//////////////////////////////////////////////////////////////////////////
//
// XFE Command utilities
//
// Is the URL a 'special' command url that translates to an FE command ?
//
//////////////////////////////////////////////////////////////////////////
/*static*/ XP_Bool
XFE_RDFUtils::ht_IsFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    return (XP_STRNCMP(url, "command:", 8) == 0);
}
//////////////////////////////////////////////////////////////////////////
/*static*/ CommandType
XFE_RDFUtils::ht_GetFECommand(HT_Resource item)
{
    const char* url = HT_GetNodeURL(item);

    if (url && XP_STRNCMP(url, "command:", 8) == 0)
    {
        return Command::convertOldRemote(url + 8);
    }
    else 
    {
        return NULL;
    }
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// RDF folder and item utilities
//
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindFolderByName(HT_Resource root,
								  char * folder_name)
{
    if (!root || !folder_name)
    {
        return NULL;
    }

    HT_Resource header;

    char * header_name = HT_GetNodeName(root);

    if(XP_STRCMP(header_name,folder_name) == 0)
    {
        return root;
    }

    
    HT_SetOpenState(root, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(root);
    HT_Resource child;
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child))
        {
            header = ht_FindFolderByName(child, folder_name);

            if(header != NULL)
            {
                HT_DeleteCursor(child_cursor);
                return header;
            }
        }
    }
    HT_DeleteCursor(child_cursor);

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindItemByAddress(HT_Resource     root,
								   const char *    target_address)
{
    if (!root || !target_address)
    {
        return NULL;
    }

    char * entry_address = HT_GetNodeURL(root);

    if (entry_address && (XP_STRCMP(target_address,entry_address) == 0))
    {
        return root;
    }

    if (HT_IsContainer(root))
    {
        HT_Resource entry;

        HT_SetOpenState(entry, PR_TRUE);
        HT_Cursor child_cursor = HT_NewCursor(entry);
        HT_Resource child;
        while ( (child = HT_GetNextItem(child_cursor)) )
        {
            HT_Resource test_entry;

            test_entry = ht_FindItemByAddress(entry,target_address);
            
            if (test_entry)
            {
                HT_DeleteCursor(child_cursor);
                return test_entry;
            }
        }
        HT_DeleteCursor(child_cursor);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindNextItem(HT_Resource current)
{
    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        return HT_GetNthItem(ht_view, index + 1);
     }
    
     return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ HT_Resource
XFE_RDFUtils::ht_FindPreviousItem(HT_Resource current)
{

    if (current)
    {
        HT_View ht_view = HT_GetView(current);
        uint32 index = HT_GetNodeIndex(ht_view, current);
        if (index > 0)
            return HT_GetNthItem(ht_view, index - 1);
    }

    return NULL;
}
//////////////////////////////////////////////////////////////////////////
/* static */ XP_Bool
XFE_RDFUtils::ht_FolderHasFolderChildren(HT_Resource header)
{
    XP_ASSERT( header != NULL );
    XP_ASSERT( HT_IsContainer(header) );

    HT_SetOpenState(header, PR_TRUE);
    HT_Cursor child_cursor = HT_NewCursor(header);
    HT_Resource child;

    // Traverse until we find a header entry
    while ( (child = HT_GetNextItem(child_cursor)) )
    {
        if (HT_IsContainer(child)) 
        {
            HT_DeleteCursor(child_cursor);
            return True;
        }
    }
    HT_DeleteCursor(child_cursor);
    return False;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Guess the title for a url address
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::guessTitle(MWContext *		context,
						 const char *		address,
						 XP_Bool			sameShell,
						 char **			guessedTitleOut,
						 time_t *			guessedLastDateOut)
{
    XP_ASSERT( context != NULL );
    XP_ASSERT( address != NULL );
    XP_ASSERT( guessedTitleOut != NULL );
    XP_ASSERT( guessedLastDateOut != NULL );
    
    // Initialize the results to begin with
    *guessedLastDateOut    = 0;
    *guessedTitleOut       = NULL;

    // If the operation occurs in the same shell, look in the history.
    if (sameShell)
    {
        // Obtain the current history entry
        History_entry * hist_entry = SHIST_GetCurrent(&context->hist);

        // Make sure the history entry matches the given address, or else
        // the caller used a the same shell with a link that is not the 
        // the proxy icon (ie, in the html area).
        if (hist_entry && (XP_STRCMP(hist_entry->address,address) == 0))
        {
            *guessedTitleOut = hist_entry->title;
            *guessedLastDateOut = hist_entry->last_access;
        }
    }

#if 0
/* Currently don't for a static way to get the bookmarks */

    // Look in the bookmarks for a dup
    if (!*guessedTitleOut)
    {
        HT_Resource test_entry = NULL;
        HT_Resource root_entry = getRootFolder();

        if (root_entry)
        {
            test_entry = ht_FindItemByAddress(root_entry, address);
        }

        if (test_entry)
        {
            *guessedTitleOut = HT_GetNodeName(test_entry);
        }
    }
#endif

    // As a last resort use the address itself as a title
    if (!*guessedTitleOut)
    {
        *guessedTitleOut = (char *) address;
    }

    // Strip the leading http:// from the title
    if (XP_STRLEN(*guessedTitleOut) > 7)
    {
        if ((XP_STRNCMP(*guessedTitleOut,"http://",7) == 0) ||
            (XP_STRNCMP(*guessedTitleOut,"file://",7) == 0))
        {
            (*guessedTitleOut) += 7;
        }
    }
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// XmString hackery
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::entryToXmStringAndFontList(HT_Resource			entry,
	Display* dpy, 
	XmString* pStr, 
	XmFontList* pFontList)
{
    XP_ASSERT( entry != NULL );
    char *name = HT_GetNodeName(entry);
    XFE_RDFUtils::utf8ToXmStringAndFontList(name, dpy, pStr, pFontList);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::utf8ToXmStringAndFontList(char* utf8str,
	Display* dpy, 
	XmString* pXmStr, 
	XmFontList* pFontList)
{

    fe_Font fe_font;
    XmFontList dontFreeFontList;

    // need to perform mid truncation here 
    // need to replace with UTF8 to XmString conversion and get FontList here

    fe_font = fe_LoadUnicodeFont(NULL, "", 0, 2, 0,0,0, 0,dpy);

    *pXmStr = fe_ConvertToXmString((unsigned char*) utf8str, CS_UTF8, fe_font, 
	XmFONT_IS_FONT, &dontFreeFontList);
    *pFontList = XmFontListCopy(dontFreeFontList);
}
//////////////////////////////////////////////////////////////////////////
/* static */ XmString
XFE_RDFUtils::formatItem(HT_Resource entry)
{
  XmString xmstring;
  char *data;

  if (HT_IsSeparator(entry))
      data = "-------------------------";
  else	
      data = HT_GetNodeName(entry);

  // need to replace with UTF8 to XmString conversion and get FontList here
    
  xmstring = XmStringCreateLtoR(data ,XmSTRING_DEFAULT_CHARSET);

  return (xmstring);
}
//////////////////////////////////////////////////////////////////////////
/* static */ void 
XFE_RDFUtils::setItemLabelString(Widget			item,
								 HT_Resource	entry)
{
    XP_ASSERT( XfeIsAlive(item) );
 
   XP_ASSERT( entry != NULL );

    XP_ASSERT( XmIsLabel(item) || 
               XmIsLabelGadget(item) ||
               XfeIsLabel(item) );


    // Create am XmString from the entry
    XmString xmname;
    XmFontList fontlist = NULL;
    XFE_RDFUtils::entryToXmStringAndFontList(entry, XtDisplay(item), &xmname, &fontlist);
	
    if (xmname != NULL)
    {
        XtVaSetValues(item,XmNlabelString,xmname,XmNfontList, fontlist, NULL);
		
        XmStringFree(xmname);
        XmFontListFree(fontlist);
    }
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Pixmap hackery
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::getPixmapsForEntry(Widget			item,
								 HT_Resource	entry,
								 Pixmap *		pixmapOut,
								 Pixmap *		maskOut,
								 Pixmap *		armedPixmapOut,
								 Pixmap *		armedMaskOut)
{
	XP_ASSERT( entry != NULL );
	XP_ASSERT( XfeIsAlive(item) );

    IconGroup *		ig = NULL;

    // Right now the only way an entry can be NULL is for the 
    // bookmarkMoreButton which kinda looks and acts like a folder so
    // we use folder pixmaps for it
    if (!entry)
    {
        ig = &BM_Folder_group;
    }
    else
    {
#ifdef NOT_YET
        if (hasCustomIcon(entry)) {} /*else {*/
#endif /*NOT_YET*/
        if (XFE_RDFUtils::ht_IsFECommand(entry))
        {
            const char* url = HT_GetNodeURL(entry);
            
            ig = IconGroup_findGroupForName(url + 8);
        } 
        else
        {
            if (HT_IsContainer(entry))
            {
                XP_Bool is_menu = False;//(entry == getMenuFolder());
                XP_Bool is_add  = False;//(entry == getAddFolder());

            
                if (is_add && is_menu)     ig = &BM_NewAndMenuFolder_group;
                else if (is_add)           ig = &BM_NewFolder_group;
                else if (is_menu)          ig = &BM_MenuFolder_group;
                else                       ig = &BM_Folder_group;
            }
            else
            {
                int url_type = NET_URL_Type(HT_GetNodeURL(entry));
                
                if (url_type == IMAP_TYPE_URL || url_type == MAILBOX_TYPE_URL)
                    ig = &BM_MailBookmark_group;
                else if (url_type == NEWS_TYPE_URL)
                    ig = &BM_NewsBookmark_group;
                else
                    ig = &BM_Bookmark_group;
            }
        }
    }

    Pixmap pixmap          = XmUNSPECIFIED_PIXMAP;
    Pixmap mask            = XmUNSPECIFIED_PIXMAP;
    Pixmap armedPixmap     = XmUNSPECIFIED_PIXMAP;
    Pixmap armedMask       = XmUNSPECIFIED_PIXMAP;

    if (ig)
    {
		Widget shell_for_colormap = 
			XfeAncestorFindByClass(item,shellWidgetClass,XfeFIND_ANY);

		XP_ASSERT( XfeIsAlive(shell_for_colormap) );

        IconGroup_createAllIcons(ig, 
                                 shell_for_colormap,
                                 XfeForeground(item),
                                 XfeBackground(item));
        
        pixmap        = ig->pixmap_icon.pixmap;
        mask          = ig->pixmap_icon.mask;
        armedPixmap   = ig->pixmap_mo_icon.pixmap;
        armedMask     = ig->pixmap_mo_icon.mask;
    }

    if (pixmapOut)
    {
        *pixmapOut = pixmap;
    }

    if (maskOut)
    {
        *maskOut = mask;
    }

    if (armedPixmapOut)
    {
        *armedPixmapOut = armedPixmap;
    }

    if (armedMaskOut)
    {
        *armedMaskOut = armedMask;
    }
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Style and layout
//
//////////////////////////////////////////////////////////////////////////
/* static */ int32
XFE_RDFUtils::getStyleForEntry(HT_Resource entry)
{
	XP_ASSERT( entry != NULL );

	int32			style = BROWSER_TOOLBAR_TEXT_ONLY;
    void *			data = NULL;

    // Get the Toolbar displaymode from HT
    HT_GetTemplateData(HT_TopNode(HT_GetView(entry)), 
					   gNavCenter->toolbarDisplayMode, 
					   HT_COLUMN_STRING, 
					   &data);

	// No value provided initially. So get it from prefs and set in HT
    if (!data)
    {
		/* int result = */ PREF_GetIntPref("browser.chrome.toolbar_style", 
									 &style);
		
       if (style == BROWSER_TOOLBAR_TEXT_ONLY)
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "text");
	   }
       else if (style == BROWSER_TOOLBAR_ICONS_ONLY)
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "pictures");
	   }
       else
	   {
		   HT_SetNodeData(HT_TopNode(HT_GetView(entry)), 
						  gNavCenter->toolbarDisplayMode, 
						  HT_COLUMN_STRING, 
						  "PicturesAndText");
	   }
    }
	// Value is found in HT
    else 
	{
		char * answer = (char *) data;
      
		if ((!XP_STRCASECMP(answer, "text")))
		{ 
			style = BROWSER_TOOLBAR_TEXT_ONLY;
		}
		else if ((!XP_STRCASECMP(answer, "icons")))
		{
			style = BROWSER_TOOLBAR_ICONS_ONLY;
		}
		else
		{
			style = BROWSER_TOOLBAR_ICONS_AND_TEXT;
		}
    }

	return style;
}
//////////////////////////////////////////////////////////////////////////
/* static */ unsigned char
XFE_RDFUtils::getButtonLayoutForEntry(HT_Resource entry,int32 style)
{
	XP_ASSERT( entry != NULL ) ;

	unsigned char		layout = XmBUTTON_LABEL_ONLY;
    void *				data = NULL;

    if (style == BROWSER_TOOLBAR_ICONS_AND_TEXT) 
	{
		// Get the Toolbar bitmap position from HT */
		HT_GetTemplateData(HT_TopNode(HT_GetView(entry)), 
						   gNavCenter->toolbarBitmapPosition, 
						   HT_COLUMN_STRING, 
						   &data);

		if (data)
		{
			char * answer = (char *) data;

			if ((!XP_STRCASECMP(answer, "top")))
			{ 
				layout = XmBUTTON_LABEL_ON_BOTTOM;
			}
			else if ((!XP_STRCASECMP(answer, "side")))
			{
				layout = XmBUTTON_LABEL_ON_RIGHT;
			}
		}
		// Value not provided. It is top for command buttons and side 
		// for personal
		else 
		{
			if (XFE_RDFUtils::ht_IsFECommand(entry))
			{
				layout = XmBUTTON_LABEL_ON_BOTTOM;
			}
			else
			{
				layout = XmBUTTON_LABEL_ON_RIGHT;
			}
		}
	}
    else if (style == BROWSER_TOOLBAR_ICONS_ONLY)
    {
		layout = XmBUTTON_PIXMAP_ONLY;
    } 
    else if (style == BROWSER_TOOLBAR_TEXT_ONLY)
    {
		layout = XmBUTTON_LABEL_ONLY;
    }

	return layout;
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
// Menu items
//
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::configureMenuPushButton(Widget item,HT_Resource entry)
{
	XP_ASSERT( XfeIsAlive(item) );
	XP_ASSERT( entry != NULL );

	// Pixmaps can only be set for XfeBmButton widgets
	if (XfeIsBmButton(item))
	{
		int32 style;
		PREF_GetIntPref("browser.chrome.style", &style);
		
		if (style == BROWSER_TOOLBAR_TEXT_ONLY)
		{
			XtVaSetValues(item,
						  XmNlabelPixmap,        XmUNSPECIFIED_PIXMAP,
						  XmNlabelPixmapMask,    XmUNSPECIFIED_PIXMAP,
						  NULL);
			
		}
		else
		{
			Pixmap pixmap;
			Pixmap mask;
			
			XFE_RDFUtils::getPixmapsForEntry(item,
											 entry,
											 &pixmap,
											 &mask,
											 NULL,
											 NULL);
			
			XtVaSetValues(item,
						  XmNlabelPixmap,        pixmap,
						  XmNlabelPixmapMask,    mask,
						  NULL);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
/* static */ void
XFE_RDFUtils::configureMenuCascadeButton(Widget item,HT_Resource entry)
{
	XP_ASSERT( XfeIsAlive(item) );
	XP_ASSERT( entry != NULL );

	// Pixmaps can only be set for XfeBmCascade widgets
	if (XfeIsBmCascade(item))
	{
		int32 style;
		PREF_GetIntPref("browser.chrome.style", &style);
		
		if (style == BROWSER_TOOLBAR_TEXT_ONLY)
		{
			XtVaSetValues(item,
						  XmNlabelPixmap,        XmUNSPECIFIED_PIXMAP,
						  XmNlabelPixmapMask,    XmUNSPECIFIED_PIXMAP,
						  XmNarmPixmap,          XmUNSPECIFIED_PIXMAP,
						  XmNarmPixmapMask,      XmUNSPECIFIED_PIXMAP,
						  NULL);
		}
		else
		{        
			Pixmap pixmap;
			Pixmap mask;
			Pixmap armedPixmap;
			Pixmap armedMask;
			
			XFE_RDFUtils::getPixmapsForEntry(item,
											 entry,
											 &pixmap,
											 &mask,
											 &armedPixmap,
											 &armedMask);
			
			Arg         av[4];
			Cardinal    ac = 0;
        
			XtSetArg(av[ac],XmNlabelPixmap,        pixmap); ac++;
			XtSetArg(av[ac],XmNlabelPixmapMask,    mask);   ac++;
			
			// Only show the armed pixmap/mask if this entry has children
			if (XfeIsAlive(XfeCascadeGetSubMenu(item)))
			{
				XtSetArg(av[ac],XmNarmPixmap,        armedPixmap); ac++;
				XtSetArg(av[ac],XmNarmPixmapMask,    armedMask);   ac++;
			}
        
			XtSetValues(item,av,ac);
		}
	}
}
//////////////////////////////////////////////////////////////////////////
/*static*/
void
XFE_RDFUtils::launchEntry(MWContext *	context,
                          HT_Resource	entry)
{
    if (!entry) return;

    if (!HT_IsContainer(entry) && !HT_IsSeparator(entry))
    {
        // Let HT handle the launch first
        if (!HT_Launch(entry, context))
        {
            char *			address = HT_GetNodeURL(entry);
            URL_Struct *	url = NET_CreateURLStruct(address,NET_DONT_RELOAD);

            XP_ASSERT( context != NULL );

            fe_reuseBrowser (context, url);
        }
    }

}
//////////////////////////////////////////////////////////////////////////
