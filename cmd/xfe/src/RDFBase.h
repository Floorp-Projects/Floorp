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
//----------------------------------------------------------------------
//
// Name:		RDFBase.h
// Description:	XFE_RDFBase class header.
//              HT Pane creation and notification management
// Author:		Stephen Lamm <slamm@netscape.com>
// Date:		Tue Jul 28 11:28:50 PDT 1998
//
//----------------------------------------------------------------------

#ifndef _xfe_rdfbase_h_
#define _xfe_rdfbase_h_

#include "htrdf.h"
#include "xp_core.h"
#include "Command.h"

typedef enum ERDFPaneMode {
   RDF_PANE_POPUP,
   RDF_PANE_DOCKED,
   RDF_PANE_STANDALONE,
   RDF_PANE_EMBEDDED
} ERDFPaneMode;


class XFE_RDFBase
{

public:
    
 	XFE_RDFBase();

	virtual ~XFE_RDFBase ();

    // Pane creation methods.
    void                  newPane               ();
    void                  newBookmarksPane      ();
    void                  newHistoryPane        ();
    void                  newToolbarPane        ();
    void                  newPaneFromURL        (MWContext *context,
                                                 char * url, 
												 int param_count = 0,
												 char **param_names = NULL,
												 char **param_values = NULL);
    void                  newPaneFromResource   (HT_Resource node);
    void                  newPaneFromResource   (RDF_Resource node);

    // Select a view of a pane.
    // Use this to set the view without creating a new pane.
    // (e.g. The toolbars share the pane created by the toolbox).
    // This does not set up the notify callback.  The original creator
    // of the pane is responsible for distributing notify events
    // to dependant views.
    void                  setHTView             (HT_View v);

    // Return true when the pane was created here.
    // Return false if the pane is only shared.
    XP_Bool               isPaneCreator         ();

    // Get the root folder of the current view.
    HT_Resource           getRootFolder         ();

    // Update the current view from the root.
    virtual void          updateRoot            ();

    // Handle HT events
    virtual void          notify      (HT_Resource n, HT_Event whatHappened);

protected:
    // HT event callback.  Setup when a new pane is created.
    static void           notify_cb   (HT_Notification ns, HT_Resource n, 
                                       HT_Event whatHappened,
                                       void *token, uint32 tokenType);

    // Called by the pane creation methods
    virtual void          startPaneCreate       ();
    virtual void          finishPaneCreate      ();
    
    virtual void          deletePane            ();

#ifdef DEBUG
    void                  debugEvent            (HT_Resource n, HT_Event e,
                                                 const char *name="HT_Event");
#endif

    HT_Pane               _ht_pane;
    HT_View               _ht_view;
    HT_Notification       _ht_ns;
};

#endif /* _xfe_rdfbase_h_ */
