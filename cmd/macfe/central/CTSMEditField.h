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

#pragma once


#include <TextEdit.h>
#include <TextServices.h>
#include <TSMTE.h>
#include <LEditField.h>


class CTSMEditField : public LEditField
	{
	
		public:

			enum 
				{
					
					class_ID = 'Tedt'
					
				};
								
			CTSMEditField();										//	¥ Default Constructor
			CTSMEditField(	LStream		*inStream);					//	¥ Stream Constructor
			CTSMEditField		(	const SPaneInfo&	inPaneInfo,
										Str255			inString,
										ResIDT			inTextTraitsID,
										Int16			inMaxChars,
										Uint8			inAttributes,
										TEKeyFilterFunc	inKeyFilter,
										LCommander*			inSuper);
																	//	¥ Parameterized Constructor
			virtual						~CTSMEditField();

			static pascal void 	PreUpdate(			TEHandle 	inTEHandle,
													Int32			inRefCon);
			static pascal void	PostUpdate( 
									TEHandle inTEHandle, 
									Int32 fixLen, 
									Int32 inputAreaStart, 
									Int32 inputAreaEnd, 
									Int32 pinStart, 
									Int32 pinEnd, 
									Int32 inRefCon );
			Int16				GetMaxChars() const { return mMaxChars; } // Make it public.

		protected:

			virtual	void				BeTarget(void);
			virtual void				DontBeTarget(void);

			virtual void				Initialize();
			
			TSMDocumentID							mTSMDocID;
			TSMTERecHandle						mTSMTEHandle;
			
			static Boolean						sInitialized;
			static Boolean						sHasTSM;
			static TSMTEPreUpdateUPP	sPreUpdateUPP;
			static TSMTEPostUpdateUPP	sPostUpdateUPP;
			
	};
