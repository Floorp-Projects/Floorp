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

#include <Appearance.h>

class CBrowserWindow;


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

			// set fg/bg/text fg colors based on the presence of assertions in HT for a node. Use
			// AM color as a fallback if no color specified in HT. Returns true if the HT color
			// is used.
		static bool SetupBackgroundColor ( HT_Resource inNode, void* inHTToken, ThemeBrush inBrush ) ;
		static bool SetupForegroundColor ( HT_Resource inNode, void* inHTToken, ThemeBrush inBrush ) ;
		static bool SetupForegroundTextColor ( HT_Resource inNode, void* inHTToken, ThemeTextColor inBrush ) ;
		static bool GetColor ( HT_Resource inNode, void* inHTToken, RGBColor * outColor ) ;

			// true if property is "yes", false if value is "no"
		static bool PropertyValueBool ( HT_Resource inNode, void* inHTToken );
		
			// call these before starting to load any URLs. HT may know how to handle them mo-better.
		static bool LaunchNode ( HT_Resource inNode ) ;
		static bool LaunchURL  ( const char* inURL, CBrowserWindow* inBrowser = NULL ) ;
		
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

		static MWContext* FindBrowserContext ( ) ;
	
		static Boolean			sIsInited;

};