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

// Miscellaneous RDF utility methods

#pragma once

#include "rdf.h"
#include "htrdf.h"

class URDFUtilities
{
	public:
		static void				StartupRDF();
		static void				ShutdownRDF();
		
		static Uint32			GetContainerSize(HT_Resource container);
	
			// PowerPlant tables are "1-based" whereas the HT view is "0-based". Convert
			// one index to the other...
		static Uint32			PPRowToHTRow ( TableIndexT inPPRow )
			{ return inPPRow - 1; }
		static Uint32			HTRowToPPRow ( TableIndexT inHTRow )
			{ return inHTRow + 1; }

			// Change HT's event masking for the lifetime of this class
		class StHTEventMasking
		{
			public:
				StHTEventMasking ( HT_Pane inPane, HT_NotificationMask inNewMask );
				~StHTEventMasking ( ) ;
			
			private:
				HT_NotificationMask mOldMask;
				HT_Pane mPane;
		};
		
	protected:
		static Boolean			sIsInited;
};