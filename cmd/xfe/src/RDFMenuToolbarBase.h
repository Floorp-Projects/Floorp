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
//
// Name:		RDFMenuToolbarBase.h
// Description:	XFE_RDFMenuToolbarBase class header.
//				Base class for components dealing with RDF.
// Author:		Ramiro Estrugo <ramiro@netscape.com>
// Date:		Tue Mar  4 03:45:16 PST 1997
//
//////////////////////////////////////////////////////////////////////////

#ifndef _xfe_rdf_menu_toolbar_base_h_
#define _xfe_rdf_menu_toolbar_base_h_

#include "NotificationCenter.h"
#include "htrdf.h"
#include "xp_core.h"
#include "RDFBase.h"

class XFE_Frame;
class XFE_RDFMenuToolbarBase;

// Used to pass data to the callbacks
typedef struct _ItemCallbackStruct
{
    XFE_RDFMenuToolbarBase * object;
    HT_Resource entry;
} ItemCallbackStruct;

//////////////////////////////////////////////////////////////////////////

class XFE_RDFMenuToolbarBase : public XFE_RDFBase
{
public:
    XFE_RDFMenuToolbarBase      (XFE_Frame * frame,
                                 XP_Bool     onlyHeaders = FALSE,
                                 XP_Bool     fancyItems = FALSE);

	virtual ~XFE_RDFMenuToolbarBase	();

    void            addEntryAfter        (const char * address,
                                          const char * title,
                                          time_t       lastAccess,
                                          HT_Resource  entryAfter);
    
    void            addEntryBefore       (const char * address,
                                          const char * title,
                                          time_t       lastAccess,
                                          HT_Resource  entryBefore);
    
    void            addEntryToFolder     (const char * address,
                                          const char * title,
                                          time_t       lastAccess,
                                          HT_Resource  header);

    void            setDropAddress       (const char * address);
    void            setDropTitle         (const char * title);
    void            setDropLastAccess    (time_t       lastAccess);

    const char *    getDropAddress       ();
    const char *    getDropTitle         ();
    time_t          getDropLastAccess    ();

    // Access methods
    XFE_Frame *     getFrame             () { return _frame; }

    virtual void    enableDropping       ();
    virtual void    disableDropping      ();
    virtual XP_Bool isDroppingEnabled    ();

    // Override RDFBase method
    void            notify               (HT_Resource n,
                                          HT_Event whatHappened);
protected:

    void getPixmapsForEntry(HT_Resource   entry,
                            Pixmap *      pixmap,
                            Pixmap *      mask,
                            Pixmap *      armedPixmap,
                            Pixmap *      armedMask);

#if 0
    /* static */ Pixmap bookmarkPixmapFromEntry(HT_Resourceentry);
    /* static */ Pixmap bookmarkMaskFromEntry(HT_Resourceentry);
#endif

#if 0
    // Access bookmark entries
    HT_Resource     getFirstEntry             ();
    HT_Resource     getTopLevelFolder         (const char * name);
#endif /*0*/

    // Create a bookmark item menu tree
    void            createItemTree            (Widget menu,HT_Resource entry);

    // Install submenu pane that controls dropping disability
    void            trackSubmenuMapping       (Widget submenu);

    // Override in derived class to configure
    virtual void    entryArmed                (Widget, HT_Resource);
    virtual void    entryDisarmed             (Widget, HT_Resource);
    virtual void    entryActivated            (Widget, HT_Resource);
    virtual void    entryCascading            (Widget, HT_Resource);
    virtual void    entryEnter                (Widget, HT_Resource);
    virtual void    entryLeave                (Widget, HT_Resource);

    // Gets called when the whole thing needs updating
    virtual void    prepareToUpdateRoot       ();

    // Gets called when the personal toolbar folder's name changes
    virtual void    updateToolbarFolderName   ();

    // Configure the items
    virtual void    configureXfeBmButton      (Widget, HT_Resource);
    virtual void    configureXfeBmCascade     (Widget, HT_Resource);
    virtual void    configureButton           (Widget, HT_Resource);
    virtual void    configureCascade          (Widget, HT_Resource);
    virtual void    configureSeparator        (Widget, HT_Resource);

    // Menu component creation methods
    Widget    createCascadeButton     (Widget menu, HT_Resource entry, 
                                       XP_Bool ignore_children);
    Widget    createPushButton        (Widget menu, HT_Resource entry);
    Widget    createSeparator         (Widget menu);
    Widget    createMoreButton        (Widget menu);

    HT_Resource getAddFolder          ();
    HT_Resource getMenuFolder         ();
	XP_Bool		getOnlyHeaders		  ();

    void        setItemLabelString    (Widget menu, HT_Resource entry);
    XmString getStringFromResource(HT_Resource );
    void getStyleAndLayout(HT_Resource , int32 *, unsigned char * );

    // Item callbacks
    static void      item_armed_cb      (Widget,XtPointer,XtPointer);
    static void      item_disarmed_cb   (Widget,XtPointer,XtPointer);
    static void      item_activated_cb  (Widget,XtPointer,XtPointer);
    static void      item_cascading_cb  (Widget,XtPointer,XtPointer);
    static void      item_enter_cb      (Widget,XtPointer,XtPointer);
    static void      item_leave_cb      (Widget,XtPointer,XtPointer);
    static void      item_free_data_cb  (Widget,XtPointer,XtPointer);
    static void      pane_mapping_eh    (Widget,XtPointer,XEvent *,Boolean *);

private:

    XFE_Frame *        _frame;                // The ancestor frame
    XP_Bool            _onlyHeaders;          // Only show headers
    XP_Bool            _fancyItems;           // Fancy items (pixmap & label)
    INTL_CharSetInfo   _charSetInfo;          // Char set info

    char *             _dropAddressBuffer;    // 
    char *             _dropTitleBuffer;      // 
    time_t             _dropLastAccess;       // 

    void             createPixmaps      ();

    Widget           getLastMoreMenu    (Widget menu);
};


#endif /* _xfe_bookmark_base_h_ */
