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
/*=============================================================================
	Save-To-Disk Context
	
	We create a context for saving items to disk and display a
	small status window for progress.
=============================================================================*/
#include "client.h"

// class NetscapeContext;
class CNSContext;

class CMimeMapper;
class LFileStream;
struct CStr255;

int PROGRESS_GetUrl (URL_Struct * request,
				int format_out, 
				const FSSpec * destination,
				Boolean delayed = FALSE);

// Are there any load-to-disk downloads happening for this context
// Call this before loading the URL if you do not want to interrupt the download
Boolean HasDownloads(MWContext * context);

/*-----------------------------------------------------------------------------
	Pipes
	These are a set of classes for implementing converters which write
	data to disk.
-----------------------------------------------------------------------------*/

class Pipe {
public:
					Pipe ();
	virtual 		~Pipe ();
	
	// base stream functions
	virtual OSErr 	Open ();
	virtual int 	Write (const char *buffer, int len);
	virtual void 	Complete ();
	virtual void 	Abort (int reason);
	
	// utilities
	NET_StreamClass * MakeStreamObject (char * name, MWContext * context);
	
private:
	// dispatch functions to our stream (for "C" NetLib)
	static int 		HandleProcess (NET_StreamClass *stream, const char *buffer, 
						int32 buffLen);
	static void 	HandleComplete (NET_StreamClass *stream);
	static void 	HandleAbort (NET_StreamClass *stream, int reason);
	static unsigned int	HandleWriteReady(NET_StreamClass *stream);
};

extern NET_StreamClass * 
NewFilePipe (int format_out, void *registration, URL_Struct * request, MWContext *context);


// NewExternal is the stream that handles FO_PRESENT of the types
// that are not displayed internally
static NET_StreamClass *NewExternalPipe (int format_out, 
							void *registration, 
							URL_Struct *url, 
							MWContext *context);

// NewExternal is the stream that handles FO_VIEW_SOURCE streams
// Fires up an application tied to Source mime type
static NET_StreamClass *NewSourcePipe (int format_out, 
							void *registration, 
							URL_Struct *url, 
							MWContext *context);

// NewLoadToDiskPipe writes incoming data to disk
static NET_StreamClass *NewLoadToDiskPipe (int format_out, 
							void *registration, 
							URL_Struct *url, 
							MWContext *context);

NET_StreamClass *NewSaveAsPipe (int format_out, 
								void *registration,
								URL_Struct *request, 
								MWContext *context);

class PlainFilePipe: public Pipe
{
	public:
					PlainFilePipe();
					PlainFilePipe(
							OSType 				creator,
							OSType 				fileType,
							URL_Struct*			url,
							CNSContext* 		inContext = NULL);

					PlainFilePipe(
							OSType 				creator,
							OSType 				fileType,
							FSSpec& 			writeTo,
							CNSContext* 		inContext = NULL);
					
		virtual 	~PlainFilePipe();
// ее stream interface
	OSErr 			Open ();
	int 			Write (const char *buffer, int buffLen);
	void 			Complete ();
	void 			Abort (int reason);
// ее misc
	void			SetTranslateLF(Boolean doTranslate);	// Should we do linefeed translation
protected:
	LFileStream	*	fFile;			// It will be deleted by CFileMgr
	Boolean			fDeleteFileObj;	// Should we delete the file object?
// Disk flushing routines
	int				fBytesWritten;	// We have an interactivity problem with 
		// hard drives and Mac's disk cache. If system file cache is large, flushing of
		// the large file buffer can result in a hang, and cause TCP connection to drop
		// So, we flush the file every 32K. Should make flush async one day
	CNSContext* mContext;
	Int32		fTotalBytes;
	Int32		fTotalWritten;
	Boolean		fTranslateLF;		// For ViewSource, we want to translate the LF to CR. FALSE by default
};

/***************************************************************************************
 * MapperFilePipe
 * file is specified by the mime mapper
 ***************************************************************************************/
class MapperFilePipe: public PlainFilePipe
{	
	public:
						MapperFilePipe(
								URL_Struct* 			url,
								CMimeMapper*			mapper,
								CNSContext* 		inContext = NULL);

						~MapperFilePipe ();
		static FSSpec	MakeSpec (CStr255& filename);
		void 			Complete ();

	private:
		CMimeMapper *	fMapper;		// Mapper for this file
};

