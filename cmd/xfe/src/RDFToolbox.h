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
// Name:		RDFToolbox.h
// Description:	XFE_RDFToolbox class header.
//				Build the toolbars based on RDF
// Author:		Stephen Lamm <slamm@netscape.com>
// Date:		Tue Jul 28 11:28:50 PDT 1998
//
//----------------------------------------------------------------------

#ifndef _xfe_rdf_toolbox_h_
#define _xfe_rdf_toolbox_h_

#include "htrdf.h"
#include "RDFBase.h"

class XFE_Frame;
class XFE_Toolbox;

class XFE_RDFToolbox : public XFE_RDFBase
{
public:
    
 	XFE_RDFToolbox(XFE_Frame * frame, XFE_Toolbox * toolbox);

	virtual ~XFE_RDFToolbox ();

    virtual void notify(HT_Resource n, HT_Event whatHappened);
private:
    XFE_Frame *   _frame;
    XFE_Toolbox * _toolbox;
};

#endif /*_xfe_rdf_toolbox_h_*/
