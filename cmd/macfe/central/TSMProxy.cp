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

#include "TSMProxy.h"
#include "proto.h"
#include "edt.h"
#include "uintl.h"
#include "intl_csi.h"


HoldUpdatesProxy::HoldUpdatesProxy(CEditView &inTextView) :
	mTextView(inTextView)
{
	mTextView.SetHoldUpdates(this);
	mStartY = 0;
	mHeight = 0;
}

HoldUpdatesProxy::~HoldUpdatesProxy()
{
	mTextView.SetHoldUpdates(nil);
	mTextView.DocumentChanged(mStartY, mHeight);
}

void HoldUpdatesProxy::DocumentChanged( int32 iStartY, int32 iHeight )
{
	if (mHeight == 0) {						// there is no range already
											// just set to the new range
		mStartY = iStartY;
		mHeight = iHeight;
		
	} else if (mHeight == -1) {				// the current range already extends to the bottom
											// should the top be moved up?
		if (iStartY < mStartY)
			mStartY = iStartY;
	
	} else if (iHeight == -1) {				// the new range extendes all the way to the bottom
											// should the top be moved up?
		mHeight = iHeight;
		if (iStartY < mStartY)
			mStartY = iStartY;
		
	} else {
	
		if (iStartY < mStartY) {
											// use the new top
			if (iStartY + iHeight > mStartY + mHeight) {
													// and the new height
				mStartY = iStartY;
				mHeight = iHeight;
				
			} else {
													// but the old height
				mHeight += mStartY - iStartY;
				mStartY = iStartY;
				
			}
			
		} else {
											// use the old top
			if (iStartY + iHeight > mStartY + mHeight) {
													// but use the new height
				mHeight = iStartY + iHeight - mStartY;
				
			}
			
		}
	}
}


AEEventHandlerUPP	HTMLInlineTSMProxy::sAEHandler 		= NewAEEventHandlerProc( AEHandlerTSM );
HTMLInlineTSMProxy	*HTMLInlineTSMProxy::sCurrentProxy 	= NULL;


#if _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
void HTMLInlineTSMProxy::PasteFromPtr(const Ptr thedata, int32 len, short hiliteStyle)
{
	if (len < 1)
		return;
	
	EDT_CharacterData *pData = EDT_NewCharacterData();
	if (pData) {
		pData->mask = TF_INLINEINPUT | TF_INLINEINPUTTHICK | TF_INLINEINPUTDOTTED;
		
		switch (hiliteStyle) {
			case kCaretPosition:
				pData->values = TF_INLINEINPUT | TF_INLINEINPUTTHICK;		// this is just a guess actually: FIX ME!!
			break;
 
 			case kRawText:
				pData->values = 0;
			break;
 
			case kSelectedRawText:
				pData->values = TF_INLINEINPUT | TF_INLINEINPUTTHICK | TF_INLINEINPUTDOTTED;
			break;
 
 			default:
 				XP_ASSERT(false);
			case kConvertedText:
				pData->values = TF_INLINEINPUT;
			break;
 
			case kSelectedConvertedText:
				pData->values = TF_INLINEINPUT | TF_INLINEINPUTTHICK;
			break;
		}
		
		EDT_SetCharacterData( mContext ,pData );
		EDT_FreeCharacterData(pData);
	}
	
	// HACK HACK HACK
	// ok, so everyone has been really helpful and all but I'm going to put this in as a hack
	// rather than try to do it "right":  unicodeString will either be "thedata" or the result
	// if we need to do unicode conversion.  We'll free this below if the pointer address has changed
	char *unicodeString = thedata;
	INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(mContext);
	int16 win_csid = INTL_GetCSIWinCSID(csi);
	if ( (win_csid == CS_UTF8) || (win_csid==CS_UTF7) ) {
	
		INTL_Encoding_ID winCSID = ScriptToEncoding( ::GetScriptManagerVariable( smKeyScript ) );
		unicodeString = (char *)INTL_ConvertLineWithoutAutoDetect( winCSID, CS_UTF8, (unsigned char *)thedata, len );
       	len = strlen(unicodeString);

	}
	
	if (len < 16) {								// can we use a small static buffer?
	
		char smallbuffer[16];
		XP_MEMCPY(smallbuffer, unicodeString, len);
		smallbuffer[len] = '\0';
		EDT_InsertText(mContext, smallbuffer);
		
	} else {
	
		char *verytemp = (char *) XP_ALLOC(len + 1);
		if (verytemp) {
			XP_MEMCPY(verytemp, unicodeString, len);
			verytemp[len] = '\0';
			EDT_InsertText(mContext, verytemp);
			XP_FREE(verytemp);
		}
		
	}
	
	// see hack alert above
	if ( unicodeString != thedata )
		XP_FREEIF(unicodeString);

}
#endif _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_

HTMLInlineTSMProxy::HTMLInlineTSMProxy( CEditView &inTextView )

	:	mTextView( inTextView )

	{

		mTSMDocID = 0;
		
		OSType	supportedType = kTextService;
		OSErr	err = ::NewTSMDocument( 1, &supportedType, &mTSMDocID, (long)(void *)this );
		ThrowIfOSErr_(err);
		mInputHoleActive = false;
		mDocActive = false;
	
	}


HTMLInlineTSMProxy::~HTMLInlineTSMProxy()
	{
	
		if ( mDocActive )
			Deactivate();	//	for a bug in TSM.  See TE27

		OSErr	err = noErr;
	
		if ( mTSMDocID )
			err = ::DeleteTSMDocument(mTSMDocID);
	
		mTSMDocID = 0;
	//	Assert_(err == noErr);
	
	}
	
	
void
HTMLInlineTSMProxy::Activate( void )
	{
			
		OSErr	err = noErr;
	
		Assert_( mDocActive == false );
	
		InstallTSMHandlers();
	
		sCurrentProxy = this;

		#ifdef Debug_Signal
			//	check to see if a bug in TSM will be encountered
			ProcessSerialNumber	psn,
								csn;
			err = GetCurrentProcess(&psn);
//			ThrowIfOSErr_(err);
			err = GetFrontProcess(&csn);
//			ThrowIfOSErr_(err);
			Assert_((psn.highLongOfPSN == csn.highLongOfPSN) && (psn.lowLongOfPSN == csn.lowLongOfPSN));
		#endif

		if ( mTSMDocID )
			err = ::ActivateTSMDocument( mTSMDocID );
		else
			err = ::UseInputWindow(NULL, true);
//		ThrowIfOSErr_(err);

		if ( err == noErr )
			mDocActive = true;

	}
	
	
void
HTMLInlineTSMProxy::Deactivate( void )
	{
	
		OSErr	err = noErr;
	
		Assert_( mDocActive );
		
		RemoveTSMHandlers();
	
		sCurrentProxy = NULL;

		err = ::DeactivateTSMDocument( mTSMDocID );

		if (err != tsmDocNotActiveErr)  //	this just seems to happen too much -- it is okay if it happens
			{
			
				Assert_( err == noErr );
			
			} 		
			
		mDocActive = false;

	}
	
	
void
HTMLInlineTSMProxy::FlushInput( void )
	{
	
		OSErr	err = noErr;
		
		Assert_( mTSMDocID != 0 );
		
		if ( mTSMDocID != 0 ) 
			{
		
				err = ::FixTSMDocument( mTSMDocID );
			
			}
	
	}
	
	
void
HTMLInlineTSMProxy::InstallTSMHandlers( void )
	{
	
		OSErr	err = noErr;
		
		err = ::AEInstallEventHandler(kTextServiceClass, kUpdateActiveInputArea, sAEHandler, kUpdateActiveInputArea, false);
		ThrowIfOSErr_(err);
		err = ::AEInstallEventHandler(kTextServiceClass, kPos2Offset, sAEHandler, kPos2Offset, false);
		ThrowIfOSErr_(err);
		err = ::AEInstallEventHandler(kTextServiceClass, kOffset2Pos, sAEHandler, kOffset2Pos, false);
		ThrowIfOSErr_(err);
	
	}

	
void
HTMLInlineTSMProxy::RemoveTSMHandlers( void )
	{
	
		OSErr err = noErr;
		
		err = ::AERemoveEventHandler(kTextServiceClass, kUpdateActiveInputArea, sAEHandler, false);
		ThrowIfOSErr_(err);
		err = ::AERemoveEventHandler(kTextServiceClass, kPos2Offset, sAEHandler, false);
		ThrowIfOSErr_(err);
		err = ::AERemoveEventHandler(kTextServiceClass, kOffset2Pos, sAEHandler, false);
		ThrowIfOSErr_(err);
	
	}
	
	
pascal OSErr	
HTMLInlineTSMProxy::AEHandlerTSM( const AppleEvent *inAppleEvent, AppleEvent *outReply, Int32 inRefCon )
	{
	
		OSErr	err = noErr;
		
		THz	oldZone = ::LMGetTheZone(),	//	Apple bug #115424?
			appZone = ::LMGetApplZone();
		::LMSetTheZone(appZone);
	
#if _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
		try 
			{
			
				Assert_( sCurrentProxy != NULL );
				
				StHandleLocker	lock(inAppleEvent->dataHandle);
				AESubDesc 		appleEvent;
				AEDescToSubDesc(inAppleEvent, &appleEvent);

//				ThrowIf_(((Int32)(void *)sCurrentProxy) != appleEvent.KeyedItem(keyAETSMDocumentRefcon).ToInt32());
				AESubDesc keySubDesc;
				AEGetKeySubDesc( &appleEvent, keyAETSMDocumentRefcon, &keySubDesc );
				long len;
				void *tsmdocrefcon = AEGetSubDescData( &keySubDesc, &len );
	
				ThrowIf_(((Int32)(void *)sCurrentProxy) != ((Int32)tsmdocrefcon));
				
				AEStream		replyStream;
				err = AEStream_OpenRecord( &replyStream, 'xxxx' );
				
				if ( sCurrentProxy != NULL )
					{
							
						switch( inRefCon )  
							{

								case kUpdateActiveInputArea:
								
									sCurrentProxy->AEUpdate(appleEvent);
									
								break;

								case kPos2Offset:
								
									sCurrentProxy->AEPos2Offset(appleEvent, replyStream);
									
								break;

								case kOffset2Pos:
								
									sCurrentProxy->AEOffset2Pos(appleEvent, replyStream);
									
								break;
								
							}
						
					}
				
				err = AEStream_CloseRecord( &replyStream );
				
				//	Transfer reply parameters to the real reply (hopefully MacOS 8 will have a way around this)
				//	ie, can simply say:
				//
				//		replyStream.Close(outReply);
				//
				StAEDescriptor	reply;
				err = AEStream_Close( &replyStream, reply );
				AESubDesc 		replySD;
				AEDescToSubDesc(reply, &replySD);
				AEKeyword		key;
				
				int32 upperBound = AECountSubDescItems( &replySD );
				for (long i = 1; i <= upperBound; i++) {
					StAEDescriptor	parm;
					AESubDesc nthSubDesc;
					err = AEGetNthSubDesc( &replySD, i, &key, &nthSubDesc );
					err = AESubDescToDesc( &nthSubDesc, key, &parm.mDesc );
//					replySD.NthItem(i, &key).ToDesc(&parm.mDesc);
					err = ::AEPutParamDesc(outReply, key, &parm.mDesc);
					ThrowIfOSErr_(err);
				}		

			} 
			
		catch ( ExceptionCode inErr ) 
			{
			
				err = inErr;
			
			} 
			
		catch ( ... ) 
			{
			
				err = paramErr;
			
			}
#endif _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_

		::LMSetTheZone(oldZone);	//	Apple bug #115424?
		
		return err;
		
	}


#if _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
void	HTMLInlineTSMProxy::AEUpdate(
	const AESubDesc	&inAppleEvent )
{
	OSErr	err;
	CEditView::OutOfFocus(&mTextView);

	HoldUpdatesProxy stopUpdatesProxy(mTextView);
	
	// if we don't already have an input hole, remember where we are
	if (!mInputHoleActive) {
		mInputHoleActive = true;
		mInputHoleStart = EDT_GetInsertPointOffset(mContext);
		mInputHoleLen = 0;
	}
	
	// get the text in the input hole
	AESubDesc keySubDesc;
	AEGetKeySubDesc( &inAppleEvent, keyAETheData, &keySubDesc );
	
	int32 textlen;
	Ptr thedata = (char *)AEGetSubDescData( &keySubDesc, &textlen );
	ThrowIf_(((Int32)(void *)sCurrentProxy) != ((Int32)(void*)thedata));
//	AESubDesc	textSD(inAppleEvent.KeyedItem(keyAETheData), typeChar);
			
	// fixLength is the number of characters which can be fixed into the buffer.
	AESubDesc fixedLengthDesc;
	AEGetKeySubDesc( &inAppleEvent, keyAEFixLength, &fixedLengthDesc );

	Int32 fixLength, lengthlen;
	fixLength = *(Int32 *)AEGetSubDescData( &fixedLengthDesc, &lengthlen );
//	Int32	fixLength = inAppleEvent.KeyedItem(keyAEFixLength).ToInt32();
	if (fixLength < 0)			// special signal to fix it all!!
		fixLength = textlen;

	mTextView.EraseCaret();
	mTextView.HideCaret(true);
	
	// if we do already have an input hole, select all the text and delete so that we start fresh
	if (mInputHoleLen) {
	
		EDT_CharacterData	*temp = EDT_GetCharacterData( mContext );

		EDT_SetInsertPointToOffset(mContext, mInputHoleStart, mInputHoleLen);
		EDT_DeletePreviousChar(mContext);
		
		if (temp) {
			if (textlen)	// if len == 0, then don't bother setting the character data because there is nothing left!
				EDT_SetCharacterData( mContext, temp );
			EDT_FreeCharacterData( temp );
		}
	}
		
	// we will handle this special case because it makes the algorithm easier to understand.
	// the input hole is going away because we are going to fix everything...
	if (fixLength == textlen) 
	{
		PasteFromPtr(thedata, fixLength, kRawText);		
		mInputHoleActive = false;
		CEditView::OutOfFocus(&mTextView);
		mTextView.HideCaret(false);
		return;
	}
	
	// we have already selected the old data, now paste in anything that needs to be fixed
	if (fixLength) {
		PasteFromPtr(thedata, fixLength, kRawText);
		mInputHoleStart = EDT_GetInsertPointOffset(mContext);	// a new starting point for our input hole
	}

	AESubDesc hiliteRangeSubDesc;
	err = AEGetKeySubDesc( &inAppleEvent, keyAEHiliteRange, &hiliteRangeSubDesc );
	XP_ASSERT( hiliteRangeSubDesc != NULL && err == noErr );
	
	if ( err == noErr) {
//	if (inAppleEvent.KeyExists(keyAEHiliteRange)) {
//		AESubDesc	hiliteSD( hiliteRangeSubDesc, typeTextRangeArray );
//		TextRangeArrayPtr	p = (TextRangeArrayPtr)hiliteSD.GetDataPtr();
		TextRangeArrayPtr p = (TextRangeArrayPtr)AEGetSubDescData( &hiliteRangeSubDesc, &textlen );
		for (Int32 i = 0; i < p->fNumOfRanges; i++) {
		
			TextRange	record;
			// we don't care about any extra information which is supposed to be encoded in the sign of any of these numbers
			record.fStart = abs(p->fRange[i].fStart);
			record.fEnd = abs(p->fRange[i].fEnd);
			record.fHiliteStyle = abs(p->fRange[i].fHiliteStyle);
			
			PasteFromPtr(thedata + fixLength + record.fStart, record.fEnd - record.fStart, record.fHiliteStyle);
		}
	}
	
	mInputHoleLen = EDT_GetInsertPointOffset(mContext) - mInputHoleStart;	// a new length for our input hole

	
	//	output
	mTextView.HideCaret(false);
	CEditView::OutOfFocus(&mTextView);
}

//	so which is it?
#define	keyAELeadingEdge	keyAELeftSide

void	HTMLInlineTSMProxy::AEPos2Offset(
	const AESubDesc	&inAppleEvent,
	AEStream		&inStream) const
{
	//	input
	Point	where;
	Boolean	dragging;
	long len;
	OSErr err;

	AESubDesc subdesc;
	err = AEGetKeySubDesc( &inAppleEvent, keyAECurrentPoint, &subdesc );
	where = *(Point *)AEGetSubDescData( &subdesc, &len );
//	inAppleEvent.KeyedItem(keyAECurrentPoint).ToPtr(typeQDPoint, &where, sizeof(where));

	
	err = AEGetKeySubDesc( &inAppleEvent, keyAEDragging, &subdesc );
	if ( AEGetSubDescType( &subdesc ) != typeNull )
		dragging = *(Boolean *)AEGetSubDescData( &subdesc, &len );
	else
		dragging = false;
	
//	AESubDesc	sd = inAppleEvent.KeyedItem(keyAEDragging);
//	if (sd.GetType() != typeNull)	//	keyAEdragging is optional
//		dragging = sd.ToBoolean();
	
	//	process
	CEditView::OutOfFocus(&mTextView);
	mTextView.FocusDraw();	//	for GlobalToLocal
	::GlobalToLocal(&where);
	CEditView::OutOfFocus(&mTextView);
	SPoint32	where32;
	mTextView.LocalToImagePoint(where, where32);
	
	LO_HitResult result;
	LO_Hit(mContext, where32.h, where32.v, false, &result, nil);
	
	if (result.type != LO_HIT_ELEMENT ||
//		result.lo_hitElement.region != LO_HIT_ELEMENT_REGION_MIDDLE ||
		result.lo_hitElement.position.element->type != LO_TEXT) {
		
			err = AEStream_WriteKey( &inStream, keyAEOffset );
//			inStream.WriteKey(keyAEOffset);
			Int32	offset = -1;
			
			err = AEStream_WriteDesc( &inStream, typeLongInteger, &offset, sizeof(offset) );
//			inStream.WriteDesc(typeLongInteger, &offset, sizeof(offset));
			
			err = AEStream_WriteKey( &inStream, keyAERegionClass );
//			inStream.WriteKey(keyAERegionClass);
			short	aShort = kTSMOutsideOfBody;
			err = AEStream_WriteDesc( &inStream, typeShortInteger, &aShort, sizeof(aShort) );
//			inStream.WriteDesc(typeShortInteger, &aShort, sizeof(aShort));
			
			return;
		}
		
	ED_BufferOffset newPosition = EDT_LayoutElementToOffset( mContext, result.lo_hitElement.position.element, result.lo_hitElement.position.position);
	
/*
	ED_BufferOffset saveSelStart, saveSelEnd;
	EDT_GetSelectionOffsets(mContext, &saveSelStart, &saveSelEnd);										// remember position
	
	EDT_PositionCaret(mContext, where32.h, where32.v );
	ED_BufferOffset newPosition = EDT_GetInsertPointOffset(mContext);
	
	EDT_SetInsertPointToOffset(mContext, saveSelStart, saveSelEnd - saveSelStart);						// restore position
*/	

	// restrict to the active range if you are dragging
	if (dragging) {
		if (newPosition < mInputHoleStart) newPosition = mInputHoleStart;
		if (newPosition > mInputHoleStart + mInputHoleLen) newPosition = mInputHoleStart + mInputHoleLen;
	}

	//	output
	err = AEStream_WriteKey( &inStream, keyAEOffset );
//	inStream.WriteKey(keyAEOffset);
	Int32	offset = newPosition;
	offset -= mInputHoleStart;
	err = AEStream_WriteDesc( &inStream, typeLongInteger, &offset, sizeof(offset) );
//	inStream.WriteDesc(typeLongInteger, &offset, sizeof(offset));
	
	err = AEStream_WriteKey( &inStream, keyAERegionClass );
//	inStream.WriteKey(keyAERegionClass);
	short	aShort = kTSMOutsideOfBody;
	
	SDimension32 sizeImage;
	SDimension16 sizeFrame;
	mTextView.GetImageSize(sizeImage);
	mTextView.GetFrameSize(sizeFrame);
	
	if ((0 <= where32.h) && (where32.h < sizeFrame.width) && (0 <= where32.v) && (where.v < sizeImage.height))
		{
		if (offset >= 0 && offset <= mInputHoleLen)
			aShort = kTSMInsideOfActiveInputArea;
		else
			aShort = kTSMInsideOfBody;
		}

	err = AEStream_WriteDesc( &inStream, typeShortInteger, &aShort, sizeof(aShort) );
//	inStream.WriteDesc(typeShortInteger, &aShort, sizeof(aShort));
}


void	HTMLInlineTSMProxy::AEOffset2Pos(
	const AESubDesc	&inAppleEvent,
	AEStream		&inStream) const
{
	OSErr	err;
	//	input
	AESubDesc subdesc;
	err = AEGetKeySubDesc( &inAppleEvent, keyAEOffset, &subdesc );
	long len;
	Int32	offset = *(Int32 *)AEGetSubDescData( &subdesc, &len );
//	Int32	offset = inAppleEvent.KeyedItem(keyAEOffset).ToInt32();
	offset += mInputHoleStart;
	
	LO_Element * element;
	int32 caretPos;
	EDT_OffsetToLayoutElement(mContext, offset, &element, &caretPos);
		
	SPoint32	where32;
	int32		veryTemp;
	GetCaretPosition( mContext, element, caretPos, &where32.h, &veryTemp, &where32.v );

	Point		where;
	mTextView.ImageToLocalPoint(where32, where);
	CEditView::OutOfFocus(&mTextView);
	mTextView.FocusDraw();	//	for LocalToGlobal
	::LocalToGlobal(&where);
	
	//	output
	err = AEStream_WriteKey( &inStream, keyAEPoint );
	err = AEStream_WriteDesc( &inStream, typeQDPoint, &where, sizeof(where) );
//	inStream.WriteKey(keyAEPoint);
//	inStream.WriteDesc(typeQDPoint, &where, sizeof(where));
}
#endif _HAVE_FIXES_FOR_REPLACING_AEGIZMOS_
