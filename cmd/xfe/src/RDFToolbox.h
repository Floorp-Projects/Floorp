/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
