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

// CAppleEventHandler.h

#include "PascalString.h"
#include "xp_mem.h"

enum KioskEnum {KioskOff = 0, KioskOn = 1};
	

class CAppleEventHandler
{
public:
	static CAppleEventHandler*	sAppleEventHandler;		// One and only instance of AEvents

	// --- Standard Constructors and Destructors
	
						CAppleEventHandler();
	virtual				~CAppleEventHandler();
	// virtual void		Initialize();

	
	// --- Top Level Apple Event Handling
	
	virtual void 		HandleAppleEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	virtual void		GetAEProperty(DescType inProperty,
									const AEDesc	&inRequestedType,
									AEDesc			&outPropertyDesc) const;
	virtual void		SetAEProperty(DescType inProperty,
							const AEDesc	&inRequestedType,
							AEDesc			&outPropertyDesc);

	// ---  AEOM support
	void				GetSubModelByUniqueID(DescType		inModelID,
									const AEDesc	&inKeyData,
									AEDesc			&outToken) const;

	static KioskEnum 	GetKioskMode(){return sAppleEventHandler->fKioskMode;}


protected:

private:

	KioskEnum			fKioskMode;
	
	void 				HandleOpenURLEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
// spy Apple Event suite
// file/URL opening + misc
	void 				HandleGetURLEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
									
	void 				HandleGetWDEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
									
	void 				HandleShowFile(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleParseAnchor(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
// Progress

	void 				HandleCancelProgress(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
// Spy window events
	void 				HandleSpyActivate(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleSpyListWindows(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleSpyGetWindowInfo(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleWindowRegistration(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
// Netscape suite
	void				HandleOpenBookmarksEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void				HandleReadHelpFileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void 				HandleGoEvent( const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);
	void				HandleOpenAddressBookEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);								

	void				HandleOpenComponentEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);								

	void				HandleCommandEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);								

	void 				HandleGetActiveProfileEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);		

	void 				HandleGetProfileImportDataEvent(const AppleEvent	&inAppleEvent,
									AppleEvent			&outAEReply,
									AEDesc				&outResult,
									long				inAENumber);		
};


/*-------------------------------------------------------------*/
//	 class EudoraSuite
//	Tools used to communicate with Eudora
//	The only real use these have is if we are operating in
//	Browser-only mode and the user wishes to use Eudora to
//	handle mail functions.
//
/*-------------------------------------------------------------*/

// --------------------------------------------------------------
/*	Some Constants used by the Eudora Suite						 */
// --------------------------------------------------------------

#define attachDouble 0
#define attachSingle 1
#define attachBinHex 2
#define attachUUencode 3

#define EU_Norm_Priority 0
#define EU_High_Priority 60
#define EU_Highest_Priority 1
#define EU_Low_Priority 160
#define EU_Lowest_Priority 200

class EudoraSuite
{
public:

	// --------------------------------------------------------------
	/*	This makes a Null AppleEvent descriptor.    
				 */
	// --------------------------------------------------------------
	static void MakeNullDesc (AEDesc *theDesc);

	// --------------------------------------------------------------
	/*	This makes a string AppleEvent descriptor.   
				 */
	// --------------------------------------------------------------
	static OSErr MakeStringDesc (Str255 theStr,AEDesc *theDesc);


	// --------------------------------------------------------------
	/*	This stuffs the required parameters into the AppleEvent. 
				 */
	// --------------------------------------------------------------


	static OSErr CreateObjSpecifier (AEKeyword theClass,AEDesc theContainer,
			AEKeyword theForm,AEDesc theData, Boolean disposeInputs,AEDesc *theSpec);

	// --------------------------------------------------------------
	/*	This creates an AEDesc for the current message.
		(The current message index = 1)     

		In:	Pointer to AEDesc to return
		Out: AEDesc constructed. */
	// --------------------------------------------------------------

	static OSErr MakeCurrentMsgSpec (AEDesc *theSpec);


	// --------------------------------------------------------------
	/*	Send a given Apple Event.  Special case for Eudora, should
		be rewritten, but it works for the moment.
		In:	AppleEvent
		Out: Event sent  */
	// --------------------------------------------------------------
	static OSErr SendEvent (AppleEvent *theEvent);

	// --------------------------------------------------------------
	/*	Create an Apple Event to be sent to Eudora
		In:	Event Class
			Event ID
			Ptr to Apple Event
		Out: Event constructed and returned.  */
	// --------------------------------------------------------------
	static OSErr MakeEvent (AEEventClass eventClass,AEEventID eventID,AppleEvent *theEvent);

	// --------------------------------------------------------------
	/*	This sets the data in a specified field. It operates on the frontmost message 
		in Eudora. It is the equivalent of sending the following AppleScript:
		set field "fieldname" of message 0 to "data"

		Examples for setting up a complete mail message:
			EudoraSuite::SendSetData("\pto",toRecipientPtr);
			EudoraSuite::SendSetData("\pcc",ccRecipientPtr);
			EudoraSuite::SendSetData("\pbcc",bccRecipientPtr);
			EudoraSuite::SendSetData("\psubject",subjectPtr);
			EudoraSuite::SendSetData("\p",bodyPtr);
			
		In:	Field to set the data in (Subject, Address, Content, etc)
			Pointer to text data.
			Size of pointer (allows us to work with XP_Ptrs.
		Out: Apple Event sent to Eudora, setting a given field.  */
	// --------------------------------------------------------------
	static OSErr SendSetData(Str31 theFieldName, Ptr thePtr, long thePtrSize);
	
	// --------------------------------------------------------------
	/*	Everything you need to tell Eudora to construct a new message
		and send it.
		In:	Pointer to the list of e mail addresses to send TO
			Pointer to the list of e mail addresses to send CC
			Pointer to the list of e mail addresses to send BCC
			Pointer to the Subject text
			Priority level of message.
			XP_HUGE_CHAR_PTR to the contents of the mail
			Pointer to an FSSpec (or null if none) for an enclosure.
		Out: Apple Events sent to Eudora telling it to construct the
			message and send it. */
	// --------------------------------------------------------------

	static OSErr  SendMessage(
		Ptr		toRecipientPtr, 
		Ptr		ccRecipientPtr,
		Ptr		bccRecipientPtr,
		Ptr		subjectPtr,
		XP_HUGE_CHAR_PTR		bodyPtr,
		long	thePriority,
		FSSpec	*theEnclosurePtr);

	static OSErr Set_Eudora_Priority(long thePriority);

};


/*-------------------------------------------------------------*/
//	 class MoreExtractFromAEDesc
//	Apple event helpers -- extension of UExtractFromAEDesc.h
//	All the miscellaneous AppleEvent helper routines.
/*-------------------------------------------------------------*/

class MoreExtractFromAEDesc
{
public:

	// --------------------------------------------------------------
	/*	Given an AppleEvent, locate a string given a keyword and
		return the string
		In:	Event to search
			AEKeyword assocaated with the string
			C string ptr
		Out: Pointer to a newly created C string returned */
	// --------------------------------------------------------------
	static void GetCString(const AppleEvent	&inAppleEvent, AEKeyword keyword, 
			char * & s, Boolean inThrowIfError = true);

	// --------------------------------------------------------------
	/*	Given an AEDesc of type typeChar, return it's string.
		In:	AEDesc containing a string
			C string ptr
		Out: Pointer to a newly created C string returned */
	// --------------------------------------------------------------
	static void TheCString(const AEDesc &inDesc, char * & outPtr);
	
	// --------------------------------------------------------------
	/*	Add an error string and error code to an AppleEvent.
		Typically used when constructing the return event when an
		error occured
		In:	Apple Event to append to
			Error string
			Error code
		Out: keyErrorNum and keyErrorSting AEDescs are added to the Event. */
	// --------------------------------------------------------------
	static void MakeErrorReturn(AppleEvent &event, const CStr255& errorString, 
			OSErr errorCode);

	// --------------------------------------------------------------
	/*	Display an error dialog if the given AppleEvent contains
		a keyErrorNumber.  a keyErrorString is optional and will be
		displayed if present
		In:	Apple Event
		Out: Error dialog displayed if error data present. */
	// --------------------------------------------------------------
	static Boolean DisplayErrorReply(AppleEvent &reply);

	// --------------------------------------------------------------
	/*	Return the process serial number of the sending process.
		In:	Apple Event send by some app.
		Out: ProcessSerialNumber of the sending app. */
	// --------------------------------------------------------------
	static ProcessSerialNumber ExtractAESender(const AppleEvent &inAppleEvent);

	static void DispatchURLDirectly(const AppleEvent &inAppleEvent);
}; // class MoreExtractFromAEDesc


/*-------------------------------------------------------------*/
//	class AEUtilities
//	Some more simple Apple Event utility routines.
/*-------------------------------------------------------------*/

class AEUtilities
{
public:

	// --------------------------------------------------------------
	/*	CreateAppleEvent
		Create a new Apple Event from scratch.
		In:	Apple Event suite
			Apple Event ID
			Ptr to return Apple Event
			ProcessSerialNumber of the target app to send event to.
		Out:A new Apple Event is created.  More data may be added to
			the event simply by calling AEPutParamDesc or AEPutParamPtr */
	// --------------------------------------------------------------
	static	OSErr CreateAppleEvent(OSType suite, OSType id, 
		AppleEvent &event, ProcessSerialNumber targetPSN);

	// --------------------------------------------------------------
	/*	Check to see if there is an error in the given AEvent.
		We simply return an OSError equiv to the error value
		in the event.  If none exists (or an error took place
		during access) we return 0.
		In:	Apple Event to test
		Out:Error value returned */
	// --------------------------------------------------------------
	static	OSErr EventHasErrorReply(AppleEvent & reply);

};
