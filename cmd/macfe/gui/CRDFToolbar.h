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

#include "CDragBar.h"									// ...is a base class
#include "CImageIconMixin.h"					// ...is a base class
#include "CRDFNotificationHandler.h"	// ...is a base class



class CRDFToolbar
		: public CDragBar,
			public CTiledImageMixin,
			public CRDFNotificationHandler

		/*
			...

			A |CRDFToolbar| has a one-to-one relationship with a particular |HT_View|.  No more
			than one FE object should ever be instantiated for this |HT_View|.
		*/
	{
		public:
			CRDFToolbar( HT_View, LView* );
			virtual ~CRDFToolbar();

		private: // Pass-by-value is not allowed.  A single |CRDFToolbar| corresponds to a single on-screen object; copying doesn't make sense.
			CRDFToolbar( const CRDFToolbar& );						// DON'T IMPLEMENT
			CRDFToolbar& operator=( const CRDFToolbar& );	// DON'T IMPLEMENT

		public:			
				// ...for |LPane|, |LView|, |CDragBar|...
			virtual void Draw( RgnHandle );

				// ...for |CRDFNotificationHandler|
			virtual void HandleNotification( HT_Notification, HT_Resource, HT_Event, void*, uint32 );

		protected:
				// ...for |CTiledImageMixin|
			virtual void ImageIsReady();
			virtual void DrawStandby( const Point&, const IconTransformType ) const;

				// PowerPlant overrides
			virtual void DrawSelf ( ) ;
			virtual void EraseBackground ( ) const;
			virtual void ShowSelf ( ) ;
			virtual void ResizeFrameBy ( SInt16 inWidth, SInt16 inHeight, Boolean inRefresh );
			
			virtual void FillInToolbar ( ) ;
			virtual void LayoutButtons ( ) ;
				
			virtual void AddHTButton ( HT_Resource inButton ) ;
			
				// helpful accessors
			HT_View HTView() { return _ht_view; }
			const HT_View HTView() const { return _ht_view; }
			HT_Resource TopNode() { return HT_TopNode(HTView()); }
			const HT_Resource TopNode() const { return HT_TopNode(HTView()); }
			
			void notice_background_changed();

		private:
			HT_View _ht_view;
	};

