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
/*---------------------------------------*/
/*																		*/
/* Name:		ToolbarDrop.cpp											*/
/* Description:	Classes to support drop stuff on toolbars.				*/
/* Author:		Ramiro Estrugo <ramiro@netscape.com>					*/
/*																		*/
/*----------------------------------------------------------------------*/



#include "PersonalToolbar.h"
#include "ToolbarDrop.h"
#include "BookmarkMenu.h"
#include "MozillaApp.h"

#include <Xfe/Cascade.h>
#include <Xfe/Button.h>
#include <Xfe/ToolBar.h>
#include <Xfe/BmButton.h>
#include <Xfe/BmCascade.h>

#ifdef DEBUG_ramiro
#define XDEBUG(x) x
#else
#define XDEBUG(x)
#endif

// cancels timer which loads homepage after sploosh screen (mozilla.c)
extern "C" void plonk_cancel();

//
// XFE_ToolbarDrop class
//

// Amount of time in ms to wait before posting a personal/quickfile popup.
// This delay is required for dnd operations that originate in a foreign
// shell and cause focus in/out events which in turn might cause window 
// managers (such as fvwm) to auti-raise a window.  This would cover the
// popup menu making it useless.
#define DROP_POST_SLEEP_LENGTH 1000

//////////////////////////////////////////////////////////////////////////
XFE_ToolbarDrop::XFE_ToolbarDrop(Widget parent)
    : XFE_DropNetscape(parent)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_ToolbarDrop::~XFE_ToolbarDrop()
{
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarDrop::targets()
{
    _numTargets=2;
    _targets=new Atom[_numTargets];

    _targets[0]=_XA_NETSCAPE_URL;
    _targets[1]=XA_STRING;

    acceptFileTargets();
}
//////////////////////////////////////////////////////////////////////////
void
XFE_ToolbarDrop::operations()
{
    // always copy - move/link irrelevant
    _operations = (unsigned int) XmDROP_COPY;
}
//////////////////////////////////////////////////////////////////////////
Boolean
XFE_ToolbarDrop::isFromSameShell()
{
	// Check if this operation occurred in the same shell as the _widget
    if (XFE_DragBase::_activeDragShell) 
	{
        Widget shell = _widget;

        while (!XtIsShell(shell))
		{
			shell=XtParent(shell);
		}

        if (shell==XFE_DragBase::_activeDragShell)
		{
            return True;
		}
    }


	return False;
}
//////////////////////////////////////////////////////////////////////////
int
XFE_ToolbarDrop::processTargets(Atom *			targets,
								const char **	data,
								int				numItems)
{
    XDEBUG(printf("XFE_ToolbarDrop::processTargets()\n"));
    
    if (!targets || !data || numItems==0)
        return FALSE;

    // open dropped files in browser

    for (int i=0;i<numItems;i++) {
        if (targets[i]==None || data[i]==NULL || strlen(data[i])==0)
            continue;

        XDEBUG(printf("  [%d] %s: \"%s\"\n",i,XmGetAtomName(XtDisplay(_widget),targets[i]),data[i]));

        if (targets[i]==_XA_FILE_NAME) 
		{
			// "file:" + filename + \0
            char *address=new char[5+strlen(data[i])+1]; 

            XP_SPRINTF(address,"file:%s",data[i]);

			if (address)
			{
				plonk_cancel();

				addEntry(address,NULL);


				delete address;
			}
        }
        
        if (targets[i]==_XA_NETSCAPE_URL) 
		{
            XFE_URLDesktopType urlData(data[i]);

            for (int j=0;j<urlData.numItems();j++) 
			{
				if (urlData.url(j))
				{
					plonk_cancel();

					addEntry(urlData.url(j),NULL);
				}
            }
        }

        if (targets[i]==XA_STRING) 
		{
			if (data[i])
			{
				plonk_cancel();

				addEntry(data[i],NULL);
			}
        }
    }
    
    return TRUE;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_ToolbarDrop::addEntry(const char * /* address */,const char * /* title */)
{
}
//////////////////////////////////////////////////////////////////////////

//
// XFE_PersonalDrop class
//

//////////////////////////////////////////////////////////////////////////
XFE_PersonalDrop::XFE_PersonalDrop(Widget					dropWidget, 
								   XFE_PersonalToolbar *	toolbar) :
    XFE_ToolbarDrop(dropWidget),
	_personalToolbar(toolbar),
	_dropWidget(NULL)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_PersonalDrop::~XFE_PersonalDrop()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalDrop::addEntry(const char * address,const char * title)
{
	XP_ASSERT( address != NULL );

	XP_ASSERT( XfeIsAlive(_dropWidget) );
	XP_ASSERT( _personalToolbar != NULL );

	char *		guessed_title = (char *) title;
	BM_Date		lastAccess = 0;

	// If no title is given, try to guess a decent one
	if (!guessed_title)
	{
		XFE_BookmarkBase::guessTitle(_personalToolbar->getFrame(),
									 _personalToolbar->getBookmarkContext(),
									 address,
									 isFromSameShell(),
									 &guessed_title,
									 &lastAccess);
	}

	XP_ASSERT( guessed_title != NULL );

	unsigned char location = _personalToolbar->getDropTargetLocation();

	// If the drop occurred on a cascade item, then we need to waid for 
	// the drop operation to complete before posting the submenu id and 
	// allowing the user to place the new bookmark.  This will happen
	// in dropCompolete().  Here, we just install the drop parameters that
	// the personal toolbar will use when adding the new bookmark.
	if (XfeIsCascade(_dropWidget))
	{
		BM_Entry * entry = (BM_Entry *) XfeUserData(_dropWidget);
		
		if (entry != NULL)
		{
			if (location == XmINDICATOR_LOCATION_BEGINNING)
			{
				_personalToolbar->addEntryBefore(address,
												 guessed_title,
												 lastAccess,
												 entry);

				// Clear drop widget so dropComplete() does not get hosed
				_dropWidget = NULL;
			}
			else if (location == XmINDICATOR_LOCATION_END)
			{
				_personalToolbar->addEntryAfter(address,
												guessed_title,
												lastAccess,
												entry);

				// Clear drop widget so dropComplete() does not get hosed
				_dropWidget = NULL;
			}
			else if (location == XmINDICATOR_LOCATION_MIDDLE)
			{
				// If the folder is empty, then just add the new bm to it
				if (BM_IsHeader(entry) && !BM_GetChildren(entry))
				{
					_personalToolbar->addEntryToFolder(address,
													   guessed_title,
													   lastAccess,
													   entry);
					
					// Clear drop widget so dropComplete() does not get hosed
					_dropWidget = NULL;
				}
				// Otherwise need to popup the bookmark placement gui later
				else
				{
					_personalToolbar->setDropAddress(address);
					_personalToolbar->setDropTitle(guessed_title);
					_personalToolbar->setDropLastAccess(lastAccess);
				}
			}
		}
	}
	// If the drop occurred on a button, then we add the new bookmark
	// at the position of the previous item
	else if (XfeIsButton(_dropWidget))
	{
		BM_Entry * entry = (BM_Entry *) XfeUserData(_dropWidget);

		if (entry)
		{
			if (location == XmINDICATOR_LOCATION_BEGINNING)
			{
				_personalToolbar->addEntryBefore(address,
												 guessed_title,
												 lastAccess,
												 entry);
			}
			else if (location == XmINDICATOR_LOCATION_END)
			{
				_personalToolbar->addEntryAfter(address,
												guessed_title,
												lastAccess,
												entry);
			}
			else
			{
				_personalToolbar->addEntry(address,guessed_title,lastAccess);
			}
		}
		
		// Clear the drop widget so that dropComplete() does not get hosed
		_dropWidget = NULL;
 	}
	// Other toolbar items - should not get here
	else
	{
		// Separators (and maybe other items) are going to be a problem
		// hmmmm...
		XP_ASSERT( 0 );

		// Clear the drop widget so that dropComplete() does not get hosed
		_dropWidget = NULL;
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalDrop::dropComplete()
{
	_personalToolbar->clearDropTargetItem();

	// If the drop widget is still alive and kicking, it means that a drop
	// occurred on a cascade item.  Here we deal with this unique case.
	if (XfeIsAlive(_dropWidget) && XfeIsSensitive(_dropWidget))
	{
		// App is now busy
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appBusyCallback);

		// Sleep for a while so that the drop site has time to "recover"
		// as well as allow time for the parent shell of the drop widget to
		// get focus if the drop originated in a different shell and 
		// the user's window manager is configured to auto-raise windows
		XfeSleep(_dropWidget,fe_EventLoop,DROP_POST_SLEEP_LENGTH);

		// Enable dropping into the personal toolbar
		_personalToolbar->enableDropping();

		// Arm and post the cascade.  Once the cascade button's submenu id
		// is posted, the bookmark menu items it manages will detect
		// that the accent has been enabled (previous statement) and
		// instead of behaving as a normal menu, will allow the user to
		// place the bookmark in an arbitrary position within the
		// personal toolbar.
 		XfeCascadeArmAndPost(_dropWidget,NULL);

		// App is not busy anymore
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appNotBusyCallback);
	}

	// Clear the drop widget since we are now done with it for sure
	_dropWidget = NULL;
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalDrop::dragIn()
{
	dragMotion();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalDrop::dragOut()
{
    _dropWidget = _personalToolbar->getDropTargetItem();

	_personalToolbar->clearDropTargetItem();
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_PersonalDrop::dragMotion()
{
	// Try to find an item at the X,Y that the motion occured
	Widget target = XfeDescendantFindByCoordinates(_widget,
												   _dropEventX,
												   _dropEventY);


	// If we found the indicator, ignore it
	if (target != _personalToolbar->getIndicatorItem())
	{
		// If an item is found, use it as the target
		if (XfeIsAlive(target))
		{
			_personalToolbar->setDropTargetItem(target,
												_dropEventX - XfeX(target));

            // The argument should really be a (BM_Entry *) 
			_personalToolbar->configureIndicatorItem(NULL);
		}
		// Otherwise use the last item
		else
		{
			Widget last = _personalToolbar->getLastItem();

			// The 10000 forces the position to be at the END of the item
			if (XfeIsAlive(last))
			{
				_personalToolbar->setDropTargetItem(last,10000);

                // The argument should really be a (BM_Entry *) 
                _personalToolbar->configureIndicatorItem(NULL);
			}
		}
	}

	// Assign the drop widget so that addEntry() can do its magic
    _dropWidget = _personalToolbar->getDropTargetItem();
}
//////////////////////////////////////////////////////////////////////////


//
// XFE_QuickfileDrop class
//

//////////////////////////////////////////////////////////////////////////
XFE_QuickfileDrop::XFE_QuickfileDrop(Widget					dropWidget,
									 XFE_BookmarkMenu *		quickfileMenu) :
    XFE_ToolbarDrop(dropWidget),
	_quickfileMenu(quickfileMenu)
{
}
//////////////////////////////////////////////////////////////////////////
XFE_QuickfileDrop::~XFE_QuickfileDrop()
{
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_QuickfileDrop::addEntry(const char * address,const char * title)
{
	XP_ASSERT( address != NULL );

	XP_ASSERT( XfeIsAlive(_widget) );
	XP_ASSERT( _quickfileMenu != NULL );

	char *		guessed_title = (char *) title;
	BM_Date		lastAccess = 0;

	// If no title is given, try to guess a decent one
	if (!guessed_title)
	{
		XFE_BookmarkBase::guessTitle(_quickfileMenu->getFrame(),
									 _quickfileMenu->getBookmarkContext(),
									 address,
									 isFromSameShell(),
									 &guessed_title,
									 &lastAccess);
	}

	XP_ASSERT( guessed_title != NULL );

 	_quickfileMenu->setDropAddress(address);
 	_quickfileMenu->setDropTitle(guessed_title);
 	_quickfileMenu->setDropLastAccess(lastAccess);
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_QuickfileDrop::dragIn()
{
	if (XfeIsSensitive(_widget))
	{
		XtVaSetValues(_widget,XmNraised,True,NULL);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_QuickfileDrop::dragOut()
{
	if (XfeIsSensitive(_widget))
	{
		XtVaSetValues(_widget,XmNraised,False,NULL);
	}
}
//////////////////////////////////////////////////////////////////////////
/* virtual */ void
XFE_QuickfileDrop::dropComplete()
{
	// If the drop widget is still alive and kicking, it means that a drop
	// occurred on a cascade item.  Here we deal with this unique case.
	if (XfeIsAlive(_widget) && XfeIsSensitive(_widget))
	{
		// App is now busy
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appBusyCallback);

		// Sleep for a while so that the drop site has time to "recover"
		// as well as allow time for the parent shell of the drop widget to
		// get focus if the drop originated in a different shell and 
		// the user's window manager is configured to auto-raise windows
		XfeSleep(_widget,fe_EventLoop,DROP_POST_SLEEP_LENGTH);

		// Enable dropping into the quickfile menu
		_quickfileMenu->enableDropping();

		// Arm and post the cascade.  Once the cascade button's submenu id
		// is posted, the bookmark menu items it manages will detect
		// that the accent has been enabled (previous statement) and
		// instead of behaving as a normal menu, will allow the user to
		// place the bookmark in an arbitrary position within the
		// personal toolbar.
 		XfeCascadeArmAndPost(_widget,NULL);

		// App is not busy anymore
		XFE_MozillaApp::theApp()->notifyInterested(XFE_MozillaApp::appNotBusyCallback);
	}
}
//////////////////////////////////////////////////////////////////////////
