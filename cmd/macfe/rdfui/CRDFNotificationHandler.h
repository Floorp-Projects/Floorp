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

//
// Mixin class that handles RDF notifcation handling
//
// How to use it: 
//		Mix this class into inheritance hierarchy and implement the HandleNotification() method.
//		Then, when your class starts up, call CreateHTPane() to get a hold of the RDF "pane."
//

#pragma once

#include "htrdf.h"
#include "auto_HT_pane.h"

class CRDFNotificationHandler
{
protected:

	virtual auto_ptr<_HT_PaneStruct> CreateHTPane ( HT_Resource inResource ) ;
	virtual auto_ptr<_HT_PaneStruct> CreateHTPane ( RDF_Resource inResource ) ;
	virtual auto_ptr<_HT_PaneStruct> CreateHTPane ( const char* inURL, unsigned int inCount, 
										char** inParamNames, char** inParamValues ) ;
	virtual auto_ptr<_HT_PaneStruct> CreateHTPane();

	virtual	HT_Notification	CreateNotificationStruct();

	virtual	void	HandleNotification(
						HT_Notification	notifyStruct,
						HT_Resource		node,
						HT_Event		event,
						void			*token,
						uint32			tokenType) = 0;
	
	static void		rdfNotifyProc(
						HT_Notification	notifyStruct,
						HT_Resource		node,
						HT_Event		event,
						void			*token,
						uint32			tokenType);
};