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

#include "CEditView.h"
#define _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_ 1
#include <AESubDescs.h>
#include <AEStream.h>


class HoldUpdatesProxy
	{
	
		public:

						HoldUpdatesProxy( CEditView &inTextView );
						~HoldUpdatesProxy();
								
			void		DocumentChanged( int32 iStartY, int32 iHeight );

		protected:

			CEditView	&mTextView;
			int32		mStartY;
			int32		mHeight;
		
	};


class	HTMLInlineTSMProxy //: public VTSMProxy
	{
	//	friend class	WTSMManager;

		public:
						HTMLInlineTSMProxy( CEditView &inTextView );
						~HTMLInlineTSMProxy();

			void		SetContext( MWContext *inContext ) 
				{ 
					mContext = inContext; 
				};

			virtual void	Activate(void);
			virtual void	Deactivate(void);
			virtual void	FlushInput(void);
			virtual void 	InstallTSMHandlers(void);
			virtual void	RemoveTSMHandlers(void);
			
			static 
			pascal OSErr	AEHandlerTSM( const AppleEvent *inAppleEvent, AppleEvent *outReply, Int32 inRefCon );

		protected:
#if _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
			virtual void	AEUpdate( const AESubDesc &inAppleEvent );
																					
			virtual void	AEPos2Offset( const AESubDesc	&inAppleEvent, AEStream &inStream ) const;
																					
			virtual void	AEOffset2Pos( const AESubDesc	&inAppleEvent, AEStream &inStream ) const;

			void			PasteFromPtr( const Ptr thedata, int32 len, short hiliteStyle );
#endif _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
											
			int				mInputHoleActive;
			ED_BufferOffset	mInputHoleStart;	// since we have this...
			int32			mInputHoleLen;		// and this.
			
			CEditView		&mTextView;
			MWContext		*mContext;	
			TSMDocumentID	mTSMDocID;
			
			Boolean			mDocActive;
			
			static AEEventHandlerUPP	sAEHandler;
			static HTMLInlineTSMProxy	*sCurrentProxy;
			
	};

