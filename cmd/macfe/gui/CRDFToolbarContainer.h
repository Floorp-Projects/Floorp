/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#pragma once

#include "auto_HT_Pane.h"							// ...for |auto_ptr<_HT_PaneStruct>|, a member
#include "CDragBarContainer.h"				// ...is a base class
#include "CRDFNotificationHandler.h"	// ...is a base class



class CRDFToolbarContainer
		: public CDragBarContainer,
			public CRDFNotificationHandler
		/*
			...

			Why didn't I just fold this functionality into |CDragBarContainer|?
			Well... |CDragBarContainer| seemed very comprehensible.  It supplies just the
			functionality of moving the individual toolbars around, and machinery very
			closely related to that.

			_This_ class strictly enhances that functionality with the idea that RDF
			specifies the attributes of the bars rather than a resource.
		*/
	{
		public:

			enum
				{
					class_ID = 'RTCt'
				};

			CRDFToolbarContainer( LStream* );
			// virtual ~CRDFToolbarContainer(); -- already virtual from bases, |auto_ptr| member means no destructor needed

		private: // Pass-by-value is not allowed.  A single |CRDFToolbarContainer| corresponds to a single on-screen object; copying doesn't make sense.
			CRDFToolbarContainer( const CRDFToolbarContainer& );						// DON'T IMPLEMENT
			CRDFToolbarContainer& operator=( const CRDFToolbarContainer& );	// DON'T IMPLEMENT


		protected: 
				// overriding the appropriate methods of |CRDFNotificationHandler|
			virtual void HandleNotification( HT_Notification, HT_Resource, HT_Event, void*, uint32 );

				// overriding the appropriate methods of |CDragBarContainer|
			virtual void BuildToolbarsPresentAtStartup ( ) ;
			virtual void RestorePlace(LStream *inPlace);
			virtual void FinishCreateSelf();

		private:
			auto_ptr<_HT_PaneStruct> _ht_root;
	};
