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

/*======================================================================================
	AUTHOR:			Ted Morris <tmorris@netscape.com> - 12 DEC 96

	MODIFICATIONS:

	Date			Person			Description
	----			------			-----------
======================================================================================*/


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "URobustCreateWindow.h"

#include <LWindow.h>
#include <LCommander.h>
#include <LControl.h>
#include <LDataStream.h>
#include <LString.h>
#include <UMemoryMgr.h>

#include "fredmem.h"

// Necessary stuff from <UReanimator.cp>

typedef	Int32	TagID;

enum {
	tag_ObjectData		= 'objd',
	tag_BeginSubs		= 'begs',
	tag_EndSubs			= 'ends',
	tag_Include			= 'incl',
	tag_UserObject		= 'user',
	tag_ClassAlias		= 'dopl',
	tag_Comment			= 'comm',
	tag_End				= 'end.',
	
	object_Null			= 'null'
};

#pragma options align=mac68k

typedef struct {
	Int16	numberOfItems;
	PaneIDT	itemID[1];
} SResList, *SResListP;

#pragma options align=reset


#pragma mark -
/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -
/*======================================================================================
	Return a newly created Window object initialized from a PPob resource. Raise an
	exception if the window cannot be created. If no exception is raised, this method
	will ALWAYS return a valid LWindow object.
======================================================================================*/

LWindow *URobustCreateWindow::CreateWindow(ResIDT inWindowID, LCommander *inSuperCommander)
{
	ThrowIf_(Memory_MemoryIsLow());
	LCommander::SetDefaultCommander(inSuperCommander);
	LAttachable::SetDefaultAttachable(nil);
	LWindow	*theWindow = nil;
	OSErr error = noErr;

	URobustCreateWindow::ReadObjects('PPob', inWindowID, &theWindow, &error);	
	ThrowIfOSErr_(error);		// theWindow not valid yet, don't bother trying to delete it.
	
	theWindow->FinishCreate();
	if ( theWindow->HasAttribute(windAttr_ShowNew) )
		theWindow->Show();

	return theWindow;
}


/*======================================================================================
	Much more robust object creation. Create new objects from the data resource. 
	Return a pointer to the first object created, or nil if no objects were created.
	If a non-nil value is returned, check the outErr value to see if the object
	is valid (i.e. no exceptions were thrown during creation); if outErr returns something
	other than noErr, the object is invalid (which means don't try to delete it).
======================================================================================*/

void URobustCreateWindow::ReadObjects(OSType inResType, ResIDT inResID, void **outFirstObject, 
									  OSErr *outErr) {

	*outFirstObject = nil;
	*outErr = noErr;
	
	Try_ {
		StResource objectRes(inResType, inResID);
		::HLockHi(objectRes.mResourceH);
		
		LDataStream objectStream(*objectRes.mResourceH, ::GetHandleSize(objectRes.mResourceH));
										
		Int16 ppobVersion;
		objectStream.ReadData(&ppobVersion, sizeof(Int16));
		
		ThrowIf_(ppobVersion != 2);
										
		URobustCreateWindow::ObjectsFromStream(&objectStream, outFirstObject);
	}
	Catch_(inErr) {
		*outErr = inErr;
		// No throw, catch the exception and return error
	}
	EndCatch_
}


/*======================================================================================
	Same as the UReanimator::ObjectsFromStream(), but returns the first object, if it 
	is created.
======================================================================================*/

void *URobustCreateWindow::ObjectsFromStream(LStream *inStream, void **outFirstObject) {

	void		*firstObject = nil;
	ClassIDT	aliasClassID = 'null';
	
								// Save current defaults
	LCommander	*defaultCommander = LCommander::GetDefaultCommander();
	LView		*defaultView = LPane::GetDefaultView();
	
	Boolean		readingTags = true;
	do {
		void	*currentObject = nil;	// Object created by current tag
		TagID	theTag = tag_End;
		inStream->ReadData(&theTag, sizeof(TagID));
		
		switch (theTag) {
		
			case tag_ObjectData: {
					// Restore default Commander and View
				LCommander::SetDefaultCommander(defaultCommander);
				LPane::SetDefaultView(defaultView);
				
					// Object data consists of a byte length, class ID,
					// and then the data for the object. We use the
					// byte length to manually set the stream marker
					// after creating the object just in case the
					// object constructor doesn't read all the data.
					
				Int32		dataLength;
				inStream->ReadData(&dataLength, sizeof(Int32));
				Int32		dataStart = inStream->GetMarker();
				ClassIDT	classID;
				inStream->ReadData(&classID, sizeof(ClassIDT));
				
				if (aliasClassID != 'null') {
						// The previous tag specified an Alias for
						// the ID of this Class
					classID = aliasClassID;
				}
				
				currentObject = URegistrar::CreateObject(classID, inStream);
				inStream->SetMarker(dataStart + dataLength, streamFrom_Start);
				
				aliasClassID = 'null';	// Alias is no longer in effect

				#ifdef Debug_Signal				
					if (currentObject == nil  &&  classID != 'null') {
						LStr255	msg("\pUnregistered ClassID: ");
						msg.Append(&classID, sizeof(classID));
						SignalPStr_(msg);
					}
				#endif
				break;
			}
				
			case tag_BeginSubs:
				currentObject = URobustCreateWindow::ObjectsFromStream(inStream, outFirstObject);
				break;
				
			case tag_EndSubs:
			case tag_End:
				readingTags = false;
				break;
				
			case tag_UserObject: {
			
					// The UserObject tag is only needed for the Constructor
					// view editing program. It tells Constructor to treat
					// the following object as if it were an object of the
					// specified superclass (which must be a PowerPlant
					// class that Constructor knows about). We don't need
					// this information here, so we just read and ignore
					// the superclass ID.
					
				ClassIDT	superClassID;
				inStream->ReadData(&superClassID, sizeof(ClassIDT));
				break;
			}
				
			case tag_ClassAlias:
			
					// The ClassAlias tag defines the ClassID the for
					// the next object in the Stream. This allows you
					// to define an object which belongs to a subclass
					// of another class, but has the same data as that
					// other class.
					
				inStream->ReadData(&aliasClassID, sizeof(ClassIDT));
				break;
				
			case tag_Comment: {
			
					// The Comment tag denotes data used by PPob editors
					// that PowerPlant ignores. Format is a long word
					// byte count followed by arbitrary hex data.
				
				Int32	commentLength;
				inStream->ReadData(&commentLength, sizeof(commentLength));
				inStream->SetMarker(commentLength, streamFrom_Marker);
				break;
			}
				
			default:
				SignalPStr_("\pUnrecognized Tag");
				readingTags = false;
				break;
		}
			
		if (firstObject == nil) {
			firstObject = currentObject;
			if ( *outFirstObject == nil ) {	// This if statement if the only modified code in 
											// this method from the UReanimator::ObjectsFromStream() 
											// method.
				*outFirstObject = firstObject;	// Store the first created object in case an 
												// exception is thrown
			}
		}
			
	} while (readingTags);
	
	return firstObject;
}


