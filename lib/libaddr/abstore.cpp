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

/* file: abstore.cpp
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <1> 05Jan1998 first implementation
** <0> 23Dec1997 first interface draft
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

#define NEEDPROTOS

#include "ldif.h"
#include "xpgetstr.h"

extern "C"
{
  extern int XP_BKMKS_IMPORT_ADDRBOOK;
  extern int XP_BKMKS_SAVE_ADDRBOOK;
}

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Store_kClassName /*i*/ = "ab_Store";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_Store_kClassName /*i*/ = "AB_Store";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/*=============================================================================
 * ab_Store: database interface
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Store::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_num viewCount = 0;
	if ( mModel_Views.IsOpenOrClosingObject() )
		viewCount = mModel_Views.CountObjects(ev);
		
	const char* fileName = (mStore_FileName)? mStore_FileName : "";
	sprintf(outXmlBuf,
		"<ab_Store:str me=\"^%lX\" row=\"#%lX\" seed=\"#%lX:%lX:%lX:%lX\" file=\"%.48s\" foot=\"%lu\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\" vs=\"%lu\"/>",
		(long) this,                         // me:st=\"^%lX
		(long) mPart_RowUid,                 // row=\"#%lX\"

		(long) mStore_NameSetSeed,           // seed=\"#%lX
		(long) mStore_ColumnSetSeed,         // :%lX
		(long) mStore_RowSetSeed,            // :%lX
		(long) mStore_RowContentSeed,        // :%lX\"
	
		fileName,                            // file=\"%.48s\"
		(long) mStore_TargetFootprint,       // foot=\"%lu\"
		
		(unsigned long) mObject_RefCount,    // rc=\"%lu\"
		this->GetObjectAccessAsString(),     // ac=\"%.9s\"
		this->GetObjectUsageAsString(),      // us=\"%.9s\"
		(unsigned long) viewCount            // views=\"%lu\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Store::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseStore(ev);
		this->MarkShut();
	}
}

void ab_Store::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab_Store>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->Print(ev,
			"<ab_Store:seeds names=\"%lu\" cols=\"%lu\" rows=\"%lu\" content=\"%lu\" />",
			(long) mStore_NameSetSeed,      // names=\"%lu\"
			(long) mStore_ColumnSetSeed,    // cols=\"%lu\"
			(long) mStore_RowSetSeed,       // rows=\"%lu\"
			(long) mStore_RowContentSeed    // content=\"%lu\"
			);
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mPart_Store->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		mModel_Views.PrintObject(ev, ioPrinter);
				
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab_Store>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Store::~ab_Store() /*i*/
{
	AB_ASSERT(mStore_FileName==0);
}
    
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Store methods

ab_Store::ab_Store(ab_Env* ev, const ab_Usage& inUsage, /*i*/
    const char* inFileName, ab_num inTargetFootprint)
    : ab_Model(ev, inUsage, ab_Session::GetGlobalSession()->NewTempRowUid(),
    	 this),
    mStore_FileName( 0 ),
    mStore_TargetFootprint( inTargetFootprint ),
    mStore_ContentAccess( ab_Object_kShut )
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ab_Store")
	
	// Note that ab_Store self-references itself through the mPart_Store
	// slot inherited from ap_Part, and this means that a store can only
	// be destroyed by explicitly closing it, because otherwise the
	// refcount will not go to zero without releasing itself.
	
	mStore_NameSetSeed = mStore_ColumnSetSeed = mStore_RowSetSeed = 
		mStore_RowContentSeed = 0;

  	if ( inTargetFootprint < ab_Store_kMinFootprint )
  		mStore_TargetFootprint = ab_Store_kMinFootprint;
  		
  	if ( ev->Good() )
  	{
	  	if ( inFileName )
	  	{
	  		mStore_FileName = ev->CopyString(inFileName);
	  		if ( !mStore_FileName )
			  	this->CloseStore(ev);
	  	}
  	}
	ab_Env_EndMethod(ev)
}


void ab_Store::CloseStore(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "CloseStore")

	char* name = mStore_FileName;
  	if ( name )
  	{
  		mStore_FileName = 0;
  		ev->FreeString(name);
  	}
  	mStore_TargetFootprint = 0;
  	
  	this->CloseModel(ev);

	ab_Env_EndMethod(ev)
}

ab_bool ab_Store::ChangeStoreFileName(ab_Env* ev, const char* inFileName) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ChangeStoreFileName")

	if ( this->IsOpenOrClosingObject() )
	{
	  	char* name = ev->CopyString(inFileName);
	  	if ( name )
	  	{
	  		if ( mStore_FileName )
		  		ev->FreeString(mStore_FileName);
		  		
	  		mStore_FileName = name;
	  	}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return ev->Good();
}

const char* ab_Store::GetStoreFileName(ab_Env* ev) const /*i*/
{
	const char* outName = 0;
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "GetStoreFileName")

	if ( this->IsOpenOrClosingObject() )
	{
		char* name = mStore_FileName;
	  	if ( name )
	  	{
	  		outName = name;
	  	}
	  	else
	  	{
	  		outName = "?name?";
#ifdef AB_CONFIG_TRACE
			if ( ev->DoTrace() )
				this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/
#ifdef AB_CONFIG_DEBUG
			ev->Break("missing store file name");
#endif /*AB_CONFIG_DEBUG*/
	  	}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outName;
}

ab_num ab_Store::GetTargetFootprint(ab_Env* ev) const /*i*/
{
	ab_num outNum = 0;
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "GetTargetFootprint")

	if ( this->IsOpenOrClosingObject() )
	{
		outNum = mStore_TargetFootprint;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpenOrClosing);

	ab_Env_EndMethod(ev)
	return outNum;
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Store port utilities using the store's top level table


/*| undo_port_closure: take apart the closure, which has the same structure
**| for both ab_Store_import_file() and ab_Store_export_file(), and if
**| no problems are found, return the store and the env.
|*/
static ab_Store*
ab_Store_undo_port_closure(void* closure, ab_Env** outEnv) /*i*/
{
	ab_Store* outStore = 0;
	ab_Env* openEnv = 0;

	ab_error_uid newError = 0;
	
	ab_Object** closureVector = (ab_Object**) closure;
	if ( closureVector )
	{
		ab_Store* store = (ab_Store*) closureVector[ 0 ];
		if ( store )
		{
			if ( store->IsOpenObject() )
			{
				ab_Env* ev = (ab_Env*) closureVector[ 1 ];
				if ( ev )
				{
					if ( ev->IsOpenObject() )
					{
						// looks okay, so return the store and env:
						openEnv = ev;
						outStore = store;
					}
					else newError = ab_Store_kFaultNotOpenEnvForImport;
				}
				else newError = ab_Store_kFaultNullEnvForImport;
			}
			else newError = ab_Store_kFaultNotOpenStoreForImport;
		}
		else newError = ab_Store_kFaultNullStoreForImport;
	}
	else newError = ab_Store_kFaultNullClosureForImport;

	if ( newError )
	{
		ab_Env* panicEnv = ab_Object::TopPanicEnv();
		if ( panicEnv )
			panicEnv->NewAbookFault(ab_Store_kFaultNullStoreForImport);
	}
	
	if ( outEnv )
		*outEnv = openEnv;
	return outStore;
}

/*| export_file: a callback function called by FE_PromptForFileName() from
**| ab_Store::ExportWithPromptForFileName().  We don't expect other ways
**| of being called.
|*/
static void
ab_Store_export_file(MWContext* context, char* fileName, void* closure) /*i*/
{
	AB_USED_PARAMS_1(context); // might be a null pointer, we don't need it
	if ( fileName )
	{
		ab_Env* ev = 0;
		ab_Store* store = ab_Store_undo_port_closure(closure, &ev);
		if ( store && ev )
		{
			store->ExportEntireStoreToNamedFile(ev, fileName);
		}
		XP_FREEIF(fileName);
	}
}

/*| import_file: a callback function called by FE_PromptForFileName() from
**| ab_Store::ImportWithPromptForFileName().  We don't expect other ways
**| of being called.
|*/
static void
ab_Store_import_file(MWContext* context, char* fileName, void* closure) /*i*/
{
	AB_USED_PARAMS_1(context); // might be a null pointer, we don't need it
	if ( fileName )
	{
		ab_Env* ev = 0;
		ab_Store* store = ab_Store_undo_port_closure(closure, &ev);
		if ( store && ev )
		{
			store->ImportEntireFileByName(ev, fileName);
		}
		XP_FREEIF(fileName);
	}
}

void ab_Store::ExportWithPromptForFileName(ab_Env* ev, MWContext* ioContext) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportWithPromptForFileName")

	ab_Object* closureVector[ 2 ]; // to hold the store and the env
	closureVector[ 0 ] = this;
	closureVector[ 1 ] = ev;
	
	FE_PromptForFileName(ioContext,
		/*prompt_string*/ XP_GetString(XP_BKMKS_SAVE_ADDRBOOK),
		/*default_path*/ 0,
		/*file_must_exist_p*/ FALSE,      // can also export to a new file
		/*directories_allowed_p*/ FALSE,  // cannot export to directory dest
		ab_Store_export_file,             // the callback function
		/*closure*/ closureVector);       // state needed inside callback

	ab_Env_EndMethod(ev)
}

void ab_Store::ImportWithPromptForFileName(ab_Env* ev, MWContext* ioContext) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportWithPromptForFileName")

	ab_Object* closureVector[ 2 ]; // to hold the store and the env
	closureVector[ 0 ] = this;
	closureVector[ 1 ] = ev;
	
	FE_PromptForFileName(ioContext,
		/*prompt_string*/ XP_GetString(XP_BKMKS_IMPORT_ADDRBOOK),
		/*default_path*/ 0,
		/*file_must_exist_p*/ TRUE,       // can only import existing file
		/*directories_allowed_p*/ FALSE,  // cannot import a directory
		ab_Store_import_file,             // the callback function
		/*closure*/ closureVector);       // state needed inside callback

	ab_Env_EndMethod(ev)
}

// guess file format:

void ab_Store::ExportEntireStoreToNamedFile(ab_Env* ev, /*i*/
	const char* inFileName)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportEntireStoreToNamedFile")
	
	ab_Thumb thumb(ev, ab_Usage::kStack);
	if ( ev->Good() )
	{
		// thumb.mThumb_FileFormat = ab_File_kLdifFormat;
		this->ExportStoreToNamedFile(ev, inFileName, &thumb);
	}
	thumb.CloseObject(ev);

	ab_Env_EndMethod(ev)
}

void ab_Store::ImportEntireFileByName(ab_Env* ev, const char* inFileName) /*i*/
	// ImportFileContent() just calls ImportContent() with a thumb
	// instance intended to convey "all the file content".
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportEntireFileByName")
	
	ab_Thumb thumb(ev, ab_Usage::kStack);
	if ( ev->Good() )
	{
		// thumb.mThumb_FileFormat = ab_File_kLdifFormat;
		this->ImportFileByName(ev, inFileName, &thumb);
	}
	thumb.CloseObject(ev);

	ab_Env_EndMethod(ev)
}

void ab_Store::ExportStoreToNamedFile(ab_Env* ev, const char* inFileName, /*i*/
	 ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportStoreToNamedFile")
	
	if ( inFileName && *inFileName )
	{
		XP_File fp = XP_FileOpen(inFileName, xpLDIFFile, XP_FILE_WRITE_BIN);
		if ( fp )
		{
			ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
				fp, inFileName, /*inFrozen*/ AB_kFalse);
			if ( file )
			{
				if ( ev->Good() )
				{
					ioThumb->mThumb_FileFormat = ab_File_kLdifFormat;
					this->ExportLdif(ev, file, ioThumb);
				}
				file->ReleaseObject(ev);
			}
		}
		if ( fp )
			XP_FileClose(fp);
	}
	else ev->NewAbookFault(ab_File_kFaultNoFileName);

	ab_Env_EndMethod(ev)
}

#define ab_Store_kReadBufferSize (4 * 1024)

void ab_Store::ImportFileByName(ab_Env* ev, const char* inFileName, /*i*/
	 ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportFileByName")
	
	if ( inFileName && *inFileName )
	{
		XP_File fp = XP_FileOpen(inFileName, xpVCardFile, XP_FILE_READ);
		if ( fp )
		{
			ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
				fp, inFileName, /*inFrozen*/ AB_kTrue);
			if ( file )
			{
				if ( ev->Good() )
				{
					ioThumb->mThumb_FileFormat = ab_File_kVCardFormat;
					this->ImportVCard(ev, file, ioThumb);
				}
				file->ReleaseObject(ev);
			}
		}
		else
		{
			fp = XP_FileOpen(inFileName, xpLDIFFile, XP_FILE_READ_BIN);
			if ( fp )
			{
				ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
					fp, inFileName, /*inFrozen*/ AB_kTrue);
				if ( file )
				{
					if ( ev->Good() )
					{
						ioThumb->mThumb_FileFormat = ab_File_kLdifFormat;
						this->ImportLdif(ev, file, ioThumb);
					}
					file->ReleaseObject(ev);
				}
			}
			else
			{
				fp = XP_FileOpen(inFileName, xpBookmarks, XP_FILE_READ);
				if ( fp )
				{
					char* buffer = (char*) ev->HeapAlloc(ab_Store_kReadBufferSize);
					if ( buffer ) 
					{
						char* checkForAddr = 0;
						int i = 0;
						
						/* check the first six lines */
						XP_FileReadLine(buffer, ab_Store_kReadBufferSize, fp);
						do
						{
							checkForAddr = XP_STRSTR(buffer, "NETSCAPE-Addressbook-file");
							if ( !checkForAddr )
								XP_FileReadLine(buffer, ab_Store_kReadBufferSize, fp);
							i++;
						} while ((i < 6) && (!checkForAddr));

						ev->HeapFree(buffer);

						if ( checkForAddr && ev->Good() )
						{
							ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
								fp, inFileName, /*inFrozen*/ AB_kTrue);
							if ( file )
							{
								if ( ev->Good() )
								{
									ioThumb->mThumb_FileFormat = ab_File_kHtmlFormat;
									this->ImportHtml(ev, file, ioThumb);
								}
								file->ReleaseObject(ev);
							}
						}
						else ev->NewAbookFault(ab_Store_kFaultUnknownImportFormat);
					}
				}
				else ev->NewAbookFault(ab_Store_kFaultUnknownImportFormat);
			}
		}
		if ( fp )
			XP_FileClose(fp);
	}
	else ev->NewAbookFault(ab_File_kFaultNoFileName);

	ab_Env_EndMethod(ev)
}

void ab_Store::ExportContent(ab_Env* ev, ab_File* ioFile, /*i*/
	 ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportContent")

  	this->ExportLdif(ev, ioFile, ioThumb);

	ab_Env_EndMethod(ev)
}

// specify import file format:
void ab_Store::ImportVCard(ab_Env* ev, ab_File* ioFile, /*i*/
	 ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportVCard")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}

/* ===== ===== ===== ===== ab_ExportHub ===== ===== ===== ===== */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_ExportHub_kClassName /*i*/ = "ab_ExportHub";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

class ab_ExportHub /*d*/ {

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public members since this whole class is used here privately

	// these ab_Object subclass instances are *not* reference counted:
	ab_Thumb*    mExportHub_Thumb;
	ab_Stream*   mExportHub_OutputStream;
	ab_File*     mExportHub_OutputFile;
	ab_Row*      mExportHub_ScratchRow;

	ab_count     mExportHub_RowCount;  // number of record rows processed
	ab_count     mExportHub_ByteCount; // number of record bytes processed
	ab_pos       mExportHub_NextPos;   // pos preceding next record to import
	            
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // non-poly private ab_ExportHub methods
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_ExportHub methods

	ab_bool NeedSecondPassForLists() const /*i*/
	{ return (mExportHub_Thumb->ListsCountedInFirstPass() != 0); }
	
	ab_bool IsPastThumbLimits() const /*i*/
	{ 
		return (
			mExportHub_RowCount > mExportHub_Thumb->mThumb_RowCountLimit ||
			mExportHub_ByteCount > mExportHub_Thumb->mThumb_ByteCountLimit
		);
	}
	
	void UpdateThumbPositionProgress() const /*i*/
	{ 
		mExportHub_Thumb->mThumb_FilePos = mExportHub_NextPos;
		mExportHub_Thumb->mThumb_RowPos += mExportHub_RowCount;
	}

	ab_ExportHub(ab_Env* ev, ab_Thumb* ioThumb,
		ab_Stream* ioOutputStream, ab_File* ioOutputFile, ab_Row* ioScratchRow);

	void ExportLdifLoop(ab_Env* ev, ab_Table* ioTable);
	
#if defined(XP_MAC)
// ````` ````` ````` `````   ````` ````` ````` ````` 
// these operators should be private but some platforms have problems linking this,
// so we only make it private on the Mac, and this catches illegal object deletes.
	
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private heap methods: heap instantiation is *NOT ALLOWED*:

	void* operator new(size_t inSize);
	void operator delete(void* ioAddress);
#endif /*XP_MAC*/
};

ab_ExportHub::ab_ExportHub(ab_Env* ev, ab_Thumb* ioThumb, /*i*/
	ab_Stream* ioOutputStream, ab_File* ioOutputFile, ab_Row* ioScratchRow)
	: mExportHub_Thumb( ioThumb ),
	mExportHub_OutputStream( ioOutputStream ),
	mExportHub_OutputFile( ioOutputFile ),
	mExportHub_ScratchRow( ioScratchRow ),
	
	mExportHub_RowCount( 0 ),
	mExportHub_ByteCount( 0 ),
	mExportHub_NextPos( 0 )
{
	ab_Env_BeginMethod(ev, ab_ExportHub_kClassName, ab_ExportHub_kClassName)
	
	if ( ioThumb && ioOutputStream && ioOutputFile && ioScratchRow )
	{
		mExportHub_NextPos = ioThumb->mThumb_FilePos;
	}
	else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	
	ab_Env_EndMethod(ev)
}

void ab_ExportHub::ExportLdifLoop(ab_Env* ev, ab_Table* ioTable) /*i*/
{
	ab_Env_BeginMethod(ev, ab_ExportHub_kClassName, "ExportLdifLoop")

	ab_Row* row = mExportHub_ScratchRow;
	AB_Row* crow = row->AsSelf();
	AB_Env* cev = ev->AsSelf();
	AB_Table* ctable = ioTable->AsSelf();
	ab_row_count totalRows = AB_Table_CountRows(ctable, cev);
	ab_row_pos rowPos = mExportHub_Thumb->mThumb_RowPos;
	if ( !rowPos ) // zero?
		mExportHub_Thumb->mThumb_RowPos = ++rowPos; // start at one
		
	if ( ev->Good() )
		mExportHub_Thumb->mThumb_ExportTotalRowCount = totalRows;

	ab_bool writeMore = AB_kTrue; // false when we notice we should stop
	mExportHub_NextPos = mExportHub_OutputStream->Tell(ev);
	
	ab_cell_size maxCell = (4 * 1024); // rather big
	
	while ( ev->Good() && rowPos <= totalRows && writeMore )
	{
		mExportHub_Thumb->mThumb_RowPos = rowPos;
		ab_row_uid uid = AB_Table_GetRowAt(ctable, cev, rowPos++);
		if ( uid )
		{
			if ( AB_Row_GrowToReadEntireTableRow(crow, cev, uid, maxCell) )
			{
				if ( row->WriteToLdifStream(ev, uid, mExportHub_OutputStream) )
				{
					ab_pos tell = mExportHub_OutputStream->Tell(ev);
					if ( tell > mExportHub_NextPos )
					{
						mExportHub_ByteCount += tell - mExportHub_NextPos;
						mExportHub_NextPos = tell;
					}
					++mExportHub_RowCount; // forward progress on row count
				}
				
				if ( this->IsPastThumbLimits() )
					writeMore = AB_kFalse; // enough progress, stop
			}
		}
	}
	if ( mExportHub_Thumb->mThumb_RowPos >= totalRows )
		mExportHub_Thumb->SetCompletelyFinished();

	++mExportHub_Thumb->mThumb_PortingCallCount;
	
	ab_Env_EndMethod(ev)
}

/* ===== ===== ===== ===== ab_ImportHub ===== ===== ===== ===== */

/* ab_ImportHub is the center of activity during a specific import
** operation.  The purpose of ab_ImportHub is to temporarily keep track of
** all information and objects used during import, and to provide any methods
** required to effect necessary proceedings during specific operations.  In
** particular, ab_ImportHub handles all format specific activities involved
** in parsing string records (say, in ldif format), and performs all actions
** necessary to import a single record into the database.
**
** The member slots in ab_ImportHub provide all the inputs and outputs needed
** to process a single input record, and to return any information that might
** need to be kept as a result of importing a single record (for example, the
** mapping of suggested uid to actual uid might be updated in the thumb's
** map-table used for this purpose).
**
** ab_ImportHub can *ONLY* be instantiated on the stack, with a lifetime
** scoped entirely inside the function instantiating it.  Because all slot
** members of ab_ImportHub point to objects with lifetimes greater than the
** ab_ImportHub instance, there is *no reference counting* of these slots.
*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_ImportHub_kClassName /*i*/ = "ab_ImportHub";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

class ab_ImportHub /*d*/ {

// ````` ````` ````` `````   ````` ````` ````` `````  
public: // public members since this whole class is used here privately

	// these ab_Object subclass instances are *not* reference counted:
	ab_Store*    mImportHub_Store;
	ab_Thumb*    mImportHub_Thumb;
	ab_Stream*   mImportHub_InputStream;
	ab_File*     mImportHub_InputFile;
	ab_Row*      mImportHub_ScratchRow;

	ab_count     mImportHub_RowCount;  // number of record rows processed
	ab_count     mImportHub_ByteCount; // number of record bytes processed
	ab_pos       mImportHub_NextPos;   // pos preceding next record to import
	
	char*  mImportHub_Type; // type of attribute
	char*  mImportHub_Value; // value of attribute
            
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // non-poly private ab_ImportHub methods

	void update_conflict(ab_Env* ev, ab_row_uid inRowUid);

	void replace_conflict(ab_Env* ev, ab_row_uid inRowUid);

	void report_conflict(ab_Env* ev, ab_String* ioRecordString);

	void resolve_import_conflict(ab_Env* ev, ab_row_uid inRowUid,
		ab_String* ioRecordString);
		
	void add_member_to_list(ab_Env* ev, AB_Table* listTable,
		const char* inDistName);
		
	AB_Table* acquire_list_table(ab_Env* ev, const char* inDistName);
            
// ````` ````` ````` `````   ````` ````` ````` `````  
public: // non-poly ab_ImportHub methods

	ab_bool IsType(const char* inAttribName) const
	{ return ( strcasecomp(inAttribName, mImportHub_Type) == 0 ); }

	ab_bool IsValue(const char* inValue) const
	{ return ( strcasecomp(inValue, mImportHub_Value) == 0 ); }

	ab_bool NeedSecondPassForLists() const /*i*/
	{ return (mImportHub_Thumb->ListsCountedInFirstPass() != 0); }

	ab_bool IsPastThumbLimits() const /*i*/
	{ 
		return (
			mImportHub_RowCount > mImportHub_Thumb->mThumb_RowCountLimit ||
			mImportHub_ByteCount > mImportHub_Thumb->mThumb_ByteCountLimit
		);
	}
	
	void UpdateThumbPositionProgress() const /*i*/
	{ 
		mImportHub_Thumb->mThumb_FilePos = mImportHub_NextPos;
		mImportHub_Thumb->mThumb_RowPos += mImportHub_RowCount;
	}

	ab_ImportHub(ab_Env* ev, ab_Store* ioStore, ab_Thumb* ioThumb,
		ab_Stream* ioInputStream, ab_File* ioInputFile, ab_Row* ioScratchRow);

	void ImportLdifFirstPassLoop(ab_Env* ev, ab_String* ioRecordString);
	void ImportLdifSecondPassLoop(ab_Env* ev, ab_String* ioRecordString);
	void ImportLdifStringRecord(ab_Env* ev, ab_String* ioRecordString);
	void ImportLdifListMembers(ab_Env* ev, ab_String* ioRecordString);
	
#if defined(XP_MAC)
// ````` ````` ````` `````   ````` ````` ````` ````` 
// these operators should be private but some platforms have problems linking this,
// so we only make it private on the Mac, and this catches illegal object deletes.
	
// ````` ````` ````` `````   ````` ````` ````` `````  
private: // private heap methods: heap instantiation is *NOT ALLOWED*:

	void* operator new(size_t inSize);
	void operator delete(void* ioAddress);
#endif /*XP_MAC*/
};

ab_ImportHub::ab_ImportHub(ab_Env* ev, ab_Store* ioStore, ab_Thumb* ioThumb, /*i*/
	ab_Stream* ioInputStream, ab_File* ioInputFile, ab_Row* ioScratchRow)
	: mImportHub_Store(ioStore)
,   mImportHub_Thumb( ioThumb )
,   mImportHub_InputStream( ioInputStream )
,   mImportHub_InputFile( ioInputFile )
,   mImportHub_ScratchRow( ioScratchRow )
	
,   mImportHub_RowCount( 0 )
,   mImportHub_ByteCount( 0 )
,   mImportHub_NextPos( 0 )
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, ab_ImportHub_kClassName)
	
	if ( ioThumb && ioInputStream && ioInputFile && ioScratchRow )
	{
		mImportHub_NextPos = ioThumb->mThumb_FilePos;
	}
	else ev->NewAbookFault(ab_Object_kFaultNullObjectParameter);
	
	ab_Env_EndMethod(ev)
}

void ab_ImportHub::ImportLdifSecondPassLoop(ab_Env* ev, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "ImportLdifSecondPassLoop")

	ab_Stream* stream = mImportHub_InputStream; // need to reset stream pos
	ab_Thumb* thumb = mImportHub_Thumb;
	
	if ( !thumb->mThumb_HavePreparedSecondPass ) // starting second pass? 
	{
		thumb->mThumb_HavePreparedSecondPass = AB_kTrue;
		thumb->mThumb_CurrentPass = 2; // note we're now in second pass
		stream->Seek(ev, /*pos*/ 0); // back to start of stream
		thumb->mThumb_FilePos = 0;
		mImportHub_NextPos = 0;
	}
	if ( thumb->mThumb_CurrentPass == 1 ) // need to fix the pass count? 
		thumb->mThumb_CurrentPass = 2; // note we're now in second pass
	
	if ( this->NeedSecondPassForLists() ) // some lists seen in first pass?
	{
		ab_bool readMore = AB_kTrue; // false when we notice we should stop
		
		ab_count timesEmpty = 0; // times empty record is read from stream
		// we count empty records to allow for any leading blank lines in stream
		
		while ( ev->Good() && readMore ) // consume another record?
		{
			stream->GetDoubleNewlineDelimitedRecord(ev, ioRecordString);
			
			if ( ev->Good() ) // no problem reading record string?
			{
				mImportHub_NextPos = stream->Tell(ev); // get pos
				ab_num recordLength = ioRecordString->GetStringLength();
				
				if ( recordLength ) // nonempty ldif record?
				{
					mImportHub_ByteCount += recordLength; // byte forward progress
					++mImportHub_RowCount; // forward progress on row count
					
					if ( this->IsPastThumbLimits() )
						readMore = AB_kFalse; // enough progress, stop
					
					// actually import this string record:
				  	this->ImportLdifListMembers(ev, ioRecordString);
				}
				else if ( ++timesEmpty > 1 ) // empty more than one time?
				{
					readMore = AB_kFalse;
					mImportHub_Thumb->SetCompletelyFinished();
				}
			}
		}
	}
	else
		mImportHub_Thumb->SetCompletelyFinished();

	ab_Env_EndMethod(ev)
}

void ab_ImportHub::ImportLdifFirstPassLoop(ab_Env* ev, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "ImportLdifFirstPassLoop")

	ab_Stream* stream = mImportHub_InputStream; // need to reset stream pos
	mImportHub_Thumb->mThumb_CurrentPass = 1;
	
	ab_bool readMore = AB_kTrue; // false when we notice we should stop
		
	ab_count timesEmpty = 0; // times empty record is read from stream
	// we count empty records to allow for any leading blank lines in stream
	
	while ( ev->Good() && readMore ) // consume another record?
	{
		stream->GetDoubleNewlineDelimitedRecord(ev, ioRecordString);
		if ( ev->Good() ) // no problem reading record string?
		{
			mImportHub_NextPos = stream->Tell(ev); // get file pos
			ab_num recordLength = ioRecordString->GetStringLength();
			
			if ( recordLength ) // nonempty ldif record?
			{
				mImportHub_ByteCount += recordLength; // byte forward progress
				++mImportHub_RowCount; // forward progress on row count
				
				if ( this->IsPastThumbLimits() )
					readMore = AB_kFalse; // enough progress, stop
				
				// actually import this string record:
			  	this->ImportLdifStringRecord(ev, ioRecordString);
			}
			else if ( ++timesEmpty > 1 ) // empty more than one time?
				readMore = AB_kFalse;
		}
	}
	ab_Env_EndMethod(ev)
}

void ab_ImportHub::replace_conflict(ab_Env* ev, ab_row_uid inRowUid) /*i*/
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "replace_conflict")
	
	ab_Row* row = mImportHub_ScratchRow;
	AB_Row_ResetTableRow(row->AsSelf(), ev->AsSelf(), inRowUid);
	
	ab_Env_EndMethod(ev)
}

void ab_ImportHub::update_conflict(ab_Env* ev, ab_row_uid inRowUid) /*i*/
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "update_conflict")
	
	ab_Row* row = mImportHub_ScratchRow;
	AB_Row_UpdateTableRow(row->AsSelf(), ev->AsSelf(), inRowUid);
	
	ab_Env_EndMethod(ev)
}

void ab_ImportHub::report_conflict(ab_Env* ev, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "report_conflict")
	
	ab_StdioFile* file = mImportHub_Thumb->GetReportFile(ev);
	if ( file && ev->Good() )
	{
		file->Write(ev, ioRecordString->GetStringContent(),
			ioRecordString->GetStringLength());
	}
	
	ab_Env_EndMethod(ev)
}

void ab_ImportHub::resolve_import_conflict(ab_Env* ev, ab_row_uid inRowUid, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "resolve_import_conflict")
	
	ab_policy policy = mImportHub_Thumb->GetImportConflictPolicy(ev);
	if ( ev->Good() )
	{
		switch ( policy )
		{
			case AB_Policy_ImportConflicts_kSignalError:
				ev->NewAbookFault(AB_Env_kFaultImportDuplicate);
				break;
		
			case AB_Policy_ImportConflicts_kIgnoreNewDuplicates:
				// do nothing (just ignore the conflict)
				break;
		
			case AB_Policy_ImportConflicts_kReportAndIgnore:
				this->report_conflict(ev, ioRecordString);
				// then ignore
				break;
		
			case AB_Policy_ImportConflicts_kReplaceOldWithNew:
				this->replace_conflict(ev, inRowUid);
				break;
		
			case AB_Policy_ImportConflicts_kReportAndReplace:
				this->report_conflict(ev, ioRecordString);
				if ( ev->Good() )
					this->replace_conflict(ev, inRowUid);
				break;
		
			case AB_Policy_ImportConflicts_kUpdateOldWithNew:
				this->update_conflict(ev, inRowUid);
				break;
				
			case AB_Policy_ImportConflicts_kReportAndUpdate:
				this->report_conflict(ev, ioRecordString);
				if ( ev->Good() )
					this->update_conflict(ev, inRowUid);
				break;
		
			default:
				ev->NewAbookFault(AB_Env_kFaultOddImportPolicy);
				break;
		}
	}

	ab_Env_EndMethod(ev)
}
	
void ab_ImportHub::add_member_to_list(ab_Env* ev, /*i*/
	AB_Table* listTable, const char* inDistName)
{
	ab_Table* table = ab_Table::AsThis(listTable);
	ab_row_uid memberUid = table->FindRowWithDistName(ev, inDistName);
	if ( memberUid )
	{
		AB_Table_AddRow(listTable, ev->AsSelf(), memberUid);
	}
}
	
AB_Table* ab_ImportHub::acquire_list_table(ab_Env* ev, /*i*/
	const char* inDistName)
{
	AB_Table* outTable = 0;
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "acquire_list_table")
	ab_Table* top = mImportHub_Store->GetTopStoreTable(ev);
	if ( top )
	{
		ab_row_uid uid = top->FindRowWithDistName(ev, inDistName);
		if ( uid )
		{
			AB_Table* ctop = top->AsSelf();
			outTable = AB_Table_AcquireRowChildrenTable(ctop, ev->AsSelf(), uid);
		}
	}
	ab_Env_EndMethod(ev)
	return outTable;
}

#define ab_ImportHub_IsUpper(c) ( (c) >= 'A' && (c) <= 'Z' )
#define ab_ImportHub_ToLower(c) ( (c) + ('a' - 'A') )

void ab_ImportHub::ImportLdifListMembers(ab_Env* ev, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "ImportLdifListMembers")
	
	char dnBuf[ 512 ]; // maximum sized dn we are prepared to handle
	ab_bool haveDn = AB_kFalse; // false until the DN attrib is read
	AB_Env* cev = ev->AsSelf();
	AB_Table* list = 0; // the list table if we ever have one
	
	// cast away const because we let ldif methods write on the string:
	char* ldifEntry = (char*) ioRecordString->GetStringContent();
	char* cursor = ldifEntry; // reference that ldif routines can munge
	char* line = 0; // another line in the ldif entry

	char** typeSlot = &mImportHub_Type;  // the type of an ldif attribute
	char** valueSlot = &mImportHub_Value; // the content of an ldif attribute
	int length = 0;  // the length  of an ldif attribute
	
	ab_bool isListEntry = AB_kFalse; // false until we see group attribute

	while ( (line = str_getline(&cursor)) != NULL && ev->Good() )
	{
		if ( str_parse_line(line, typeSlot, valueSlot, &length) != 0 )
			continue; // parse error: continue with next loop iteration
					
		const char* value = mImportHub_Value;
		// this->TokenizeType(ev); // tokenize later to reduce string compares
		
		ab_u1 firstTypeByte = (ab_u1) *mImportHub_Type;
		if ( ab_ImportHub_IsUpper(firstTypeByte) ) // is upper?
			firstTypeByte = ab_ImportHub_ToLower(firstTypeByte);
		
		switch ( firstTypeByte )
		{
			case 'd':
				if ( this->IsType("dn") ) // distinuished name
				{
					haveDn = AB_kTrue;
					XP_SPRINTF(dnBuf, "%.500s", value); // note size assumption
				}
				break; // 'd'
							
			case 'm':
				if ( this->IsType("member") )
				{
					if ( !list && haveDn ) // have not yet acquire the list table?
						list = this->acquire_list_table(ev, dnBuf);
						
					if ( list )
						this->add_member_to_list(ev, list, value);
				}
				break; // 'm'
			
			case 'o':
				if ( this->IsType("objectclass") ) 
				{
					if ( this->IsValue("groupofuniquenames") || this->IsValue("groupOfNames") )
					{
						isListEntry = AB_kTrue;
						if ( !list && haveDn )
							list = this->acquire_list_table(ev, dnBuf);
					}
				}
				break; // 'o'
							
			case 'u':
				if ( this->IsType("uniquemember") )
				{
					if ( !list && haveDn ) // have not yet acquire the list table?
						list = this->acquire_list_table(ev, dnBuf);
						
					if ( list )
						this->add_member_to_list(ev, list, value);
				}
				break; // 'u'
				
			default:
				break; // default
		}
	}
	if ( isListEntry )
		++mImportHub_Thumb->mThumb_ListCountInSecondPass;
	
	if ( list ) // need to release the list table?
		AB_Table_Release(list, cev);

	ab_Env_EndMethod(ev)
}

void ab_ImportHub::ImportLdifStringRecord(ab_Env* ev, /*i*/
	ab_String* ioRecordString)
{
	ab_Env_BeginMethod(ev, ab_ImportHub_kClassName, "ImportLdifStringRecord")
	
	ab_Row* row = mImportHub_ScratchRow;
	if ( row->ClearCells(ev) ) // no problem clearing the old row cell content?
	{
		// cast away const because we let ldif methods write on the string:
		char* ldifEntry = (char*) ioRecordString->GetStringContent();
		char* cursor = ldifEntry; // reference that ldif routines can munge
		char* line = 0; // another line in the ldif entry

		char** typeSlot = &mImportHub_Type;  // the type of an ldif attribute
		char** valueSlot = &mImportHub_Value; // the content of an ldif attribute
		int length = 0;  // the length  of an ldif attribute
		
		ab_bool addCell = AB_kTrue; // yes, add any cells as needed
		ab_bool isListEntry = AB_kFalse; // false until we see group attribute
	
		while ( (line = str_getline(&cursor)) != NULL && ev->Good() )
		{
			if ( str_parse_line(line, typeSlot, valueSlot, &length) != 0 )
				continue; // parse error: continue with next loop iteration
						
			const char* value = mImportHub_Value;
			// this->TokenizeType(ev); // tokenize later to reduce string compares
			
			ab_u1 firstTypeByte = (ab_u1) *mImportHub_Type;
			if ( ab_ImportHub_IsUpper(firstTypeByte) ) // is upper?
				firstTypeByte = ab_ImportHub_ToLower(firstTypeByte);
			
			switch ( firstTypeByte )
			{
				case 'c':
					if ( this->IsType("cn") || this->IsType("commonname") )
						row->WriteCell(ev, value, AB_Column_kFullName);

					else if ( this->IsType("countryName") )
						row->WriteCell(ev, value, AB_Column_kCountry);

					else if ( this->IsType("charset") )
						row->WriteCell(ev, value, AB_Column_kCharSet);
					break; // 'c'

				case 'd':
					if ( this->IsType("dn") ) // distinuished name
						row->WriteCell(ev, value, AB_Column_kDistName);
						
					else if ( this->IsType("description") )
						row->WriteCell(ev, value, AB_Column_kInfo);
					break; // 'd'
					
				case 'f':
				
					if ( this->IsType("facsimiletelephonenumber") )
						row->WriteCell(ev, value, AB_Column_kFax);
					break; // 'f'
					
				case 'g':
					if ( this->IsType("givenname") )
						row->WriteCell(ev, value, AB_Column_kGivenName);
					break; // 'g'
				
				case 'h':
					if ( this->IsType("homephone") )
						row->WriteCell(ev, value, AB_Column_kHomePhone);
					break; // 'h'
				
				case 'l':
					if ( this->IsType("l") || this->IsType("locality") )
						row->WriteCell(ev, value, AB_Column_kLocality);
				
					break; // 'l'
				
				case 'm':
					if ( this->IsType("mail") )
						row->WriteCell(ev, value, AB_Column_kEmail);
				
					break; // 'm'
				
				case 'o':
					if ( this->IsType("o") ) // organization
						row->WriteCell(ev, value, AB_Column_kCompanyName);

					else if ( this->IsType("objectclass") ) 
					{
						if ( this->IsValue("person") ) // objectclass == person?
							row->PutBoolCol(ev, AB_kTrue, AB_Column_kIsPerson,
								addCell);
						else if ( this->IsValue("groupofuniquenames") )
						{
							row->PutBoolCol(ev, AB_kFalse, AB_Column_kIsPerson,
								addCell);
							isListEntry = AB_kTrue;
						}
						else if ( this->IsValue("groupOfNames") )
						{
							row->PutBoolCol(ev, AB_kFalse, AB_Column_kIsPerson,
								addCell);
							isListEntry = AB_kTrue;
						}
					}
					break; // 'o'
					
				case 'p':
					if ( this->IsType("postalcode") )
						row->WriteCell(ev, value, AB_Column_kZip);
						
					else if ( this->IsType("postOfficeBox") )
						row->WriteCell(ev, value, AB_Column_kPostalAddress);
					break; // 'p'
					
				case 'r':
					if ( this->IsType("rfc822mailbox") )
						row->WriteCell(ev, value, AB_Column_kEmail);
					break; // 'r'
					
				case 's':
					if ( this->IsType("sn") || this->IsType("surname") )
						row->WriteCell(ev, value, AB_Column_kFamilyName);

					else if ( this->IsType("st") )
						row->WriteCell(ev, value, AB_Column_kRegion);
						
					else if ( this->IsType("streetaddress") )
						row->WriteCell(ev, value, AB_Column_kStreetAddress);
					break; // 's'
					
				case 't':
					if ( this->IsType("title") )
						row->WriteCell(ev, value, AB_Column_kTitle);
						
					else if ( this->IsType("telephonenumber") )
						row->WriteCell(ev, value, AB_Column_kWorkPhone);
					break; // 't'
					
				case 'x':
					if ( this->IsType("xmozillanickname") )
						row->WriteCell(ev, value, AB_Column_kNickname);

					else if ( this->IsType("xmozillausehtmlmail") )
					{
						const char* v = value;
						ab_bool html = (*v == 't' || *v == 'T');
						row->PutBoolCol(ev, AB_kTrue, AB_Column_kHtmlMail,
							addCell);
					}

					else if ( this->IsType("xmozillaconference") )
						row->WriteCell(ev, value, AB_Column_kCoolAddress);

					else if ( this->IsType("xmozillauseconferenceserver") )
						row->WriteCell(ev, value, AB_Column_kUseServer);
					break; // 'x'
					
				default:
					break; // default
			}
		}
		
		if ( ev->Good() ) // no errors during record parsing and conversion?
		{
			ab_row_uid existingRow = row->FindRowUid(ev);
			if ( !existingRow && ev->Good() ) // new row in table?
			{
				AB_Env* capiEnv = ev->AsSelf();
				AB_Row* capiRow = row->AsSelf();
				AB_Row_NewTableRowAt(capiRow, capiEnv, /*inPos*/ 0);
			}
			else this->resolve_import_conflict(ev, existingRow, ioRecordString);
			
			if ( isListEntry ) // another list to revisit in the second pass?
				++mImportHub_Thumb->mThumb_ListCountInFirstPass;
		}
	}

	ab_Env_EndMethod(ev)
}


#define ab_Store_kDefaultBatchEvents 100

void ab_Store::ImportLdif(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportLdif")

#ifdef AB_CONFIG_DEBUG
	//ab_Store::UseGobsOfMemoryForAddressBookSpeed(ev);
#endif /*AB_CONFIG_DEBUG*/

	ab_num pass = ioThumb->mThumb_CurrentPass;
	ab_pos eof = ioFile->Length(ev);
	
	if ( ev->Good() && pass <= ab_Thumb_kMaxRealPass )
	{
		if ( pass ) // started first pass?
		{
			if ( ioThumb->mThumb_FilePos >= eof ) // pass exhausted?
			{
				if ( ++pass == 2 ) // now starting second pass?
				{
					if ( ioThumb->ListsCountedInFirstPass() == 0 ) // no 2nd?
						ioThumb->SetCompletelyFinished();
					else if ( !ioThumb->mThumb_HavePreparedSecondPass ) // prep? 
					{
						ioThumb->mThumb_HavePreparedSecondPass = AB_kTrue;
						ioThumb->mThumb_FilePos = 0; // reset file pos to zero
					}
				}
			}
		}
		else
			pass = 1; // starting 1st pass
			 
		ioThumb->mThumb_CurrentPass = pass;
	}

	if ( ev->Good() && pass <= ab_Thumb_kMaxRealPass ) // more file content?
	{
		ioThumb->mThumb_ImportFileLength = eof; // need for reporting progress
		
		ab_num bufSize = ioThumb->PickHeuristicStreamBufferSize();
		ab_bool frozen = AB_kTrue;
		ab_Stream stream(ev, ab_Usage::kStack, ioFile, bufSize, frozen);
		if ( ev->Good() ) // stream created okay?
		{
			stream.Seek(ev, ioThumb->mThumb_FilePos);
			if ( ev->Good() ) // seek was good?
			{
				ab_Table* topTable = this->GetTopStoreTable(ev);
				if ( topTable && ev->Good() )
				{
					ab_Row row(ev, ab_Usage::kStack, topTable, /*hint*/ 64);
					ab_String record(ev, ab_Usage::kStack, /*string*/ 0);
									
					ab_ImportHub hub(ev, this, ioThumb, &stream, ioFile, &row);
					if ( ev->Good() )
					{
						this->BeginModelFlux(ev); // SUSPEND notification updates
						this->StartBatchMode(ev, ab_Store_kDefaultBatchEvents);
						
						// before resuming first or second pass, see if done:
						if ( !ioThumb->IsProgressFinished() )
						{
							if ( ioThumb->StillOnFirstPassThroughFile() )
							{
								hub.ImportLdifFirstPassLoop(ev, &record);
								if ( ev->Good() && !hub.IsPastThumbLimits() )
								{
									ab_bool dejaVu = // have we been here before?
										( ioThumb->mThumb_PortingCallCount > 1 );
									ioThumb->mThumb_CurrentPass = 2; // next pass
									if ( !dejaVu ) // want to perform next pass?
									{
										hub.ImportLdifSecondPassLoop(ev, &record);
										if ( ev->Good() && !hub.IsPastThumbLimits() )
										{
											// show that we're completely done:
											ioThumb->SetCompletelyFinished();
										}
									}
								}
							}
							else if ( hub.NeedSecondPassForLists() ) 
							{
								hub.ImportLdifSecondPassLoop(ev, &record);
								if ( ev->Good() && !hub.IsPastThumbLimits() )
								{
									// show that we're completely done:
									ioThumb->SetCompletelyFinished();
								}
							}
							else
							{
								// show that we're completely done:
								ioThumb->SetCompletelyFinished();
							}
						}
												
						this->EndBatchMode(ev);
						this->SaveStoreContent(ev); // COMMIT all the new rows
						this->EndModelFlux(ev); // RESUME notification updates

						hub.UpdateThumbPositionProgress();
					}
					
					record.CloseObject(ev);
					row.CloseObject(ev);
				}
			}
		}
		stream.Flush(ev);
		stream.CloseStream(ev);
	}
	++ioThumb->mThumb_PortingCallCount; // notice ImportLdif() called many times

	ab_Env_EndMethod(ev)
}

void ab_Store::ImportHtml(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportHtml")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}

void ab_Store::ImportXml(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportXml")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}

void ab_Store::ImportTabDelimited(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ImportTabDelimited")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}

// specify export file format:
void ab_Store::ExportLdif(ab_Env* ev, ab_File* ioFile, /*i*/
	 ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportLdif")

	ab_num bufSize = ab_Thumb_kNormalSteamBufSize; // expect a lot
	ab_bool frozen = AB_kFalse; // we need to write this file
	ab_Stream stream(ev, ab_Usage::kStack, ioFile, bufSize, frozen);
	if ( ev->Good() ) // stream created okay?
	{
		stream.Seek(ev, ioThumb->mThumb_FilePos);
		if ( ev->Good() ) // seek was good?
		{
			ab_Table* topTable = this->GetTopStoreTable(ev);
			if ( topTable && ev->Good() )
			{
				ab_Row row(ev, ab_Usage::kStack, topTable, /*hint*/ 64);
								
				ab_ExportHub hub(ev, ioThumb, &stream, ioFile, &row);
				if ( ev->Good() )
				{
					hub.ExportLdifLoop(ev, topTable);
					hub.UpdateThumbPositionProgress();
				}
				row.CloseObject(ev);
			}
		}
	}
	stream.CloseObject(ev);

	ab_Env_EndMethod(ev)
}

void ab_Store::ExportXml(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportXml")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}

void ab_Store::ExportTabDelimited(ab_Env* ev, ab_File* ioFile, /*i*/
	ab_Thumb* ioThumb)
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "ExportTabDelimited")

	AB_USED_PARAMS_2(ioFile,ioThumb);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
}


// ===== ===== start random ldif code using IronDoc pseudo RNG ===== =====
#define FeU4_kMersenne   /*i*/ ((long) 0x7FFFFFFF) /* mersenne prime */
#define FeU4_kMultiplier /*i*/ ((long) 48271)	   /* prime multiplier */
#define FeU4_kQuotient   /*i*/ ((long) 44488)	   /* mersenne / multiplier */
#define FeU4_kRemainder  /*i*/ ((long) 3399)       /* mersenne % multiplier */

ab_i4 AbI4_Random(ab_i4 self) /*i*/ // IronDoc pseudo RNG
{
	if ( self ) /* is self non-zero (we will not return zero)? */
	{
		long high = self / FeU4_kQuotient;
		long low = self % FeU4_kQuotient;
		long test = (FeU4_kMultiplier * low) - (FeU4_kRemainder * high);
		self = ( test > 0 )? test : test + FeU4_kMersenne;
	}
	else
		self = 1; /* never return zero */
		
	return self;
}


ab_u4 ab_Env_HashKey(ab_Env* ev, ab_map_int inKey) /*i*/ // IronDoc pseudo RNG
{
	AB_USED_PARAMS_1(ev);

	ab_i4 seed = (ab_i4) inKey;
	if ( seed ) /* is seed non-zero (we will not return zero)? */
	{
		long high = seed / FeU4_kQuotient;
		long low = seed % FeU4_kQuotient;
		long test = (FeU4_kMultiplier * low) - (FeU4_kRemainder * high);
		seed = ( test > 0 )? test : test + FeU4_kMersenne;
	}
	else
		seed = 1; /* never return zero */
		
	return (ab_u4) seed;
}

ab_bool ab_Env_EqualKey(ab_Env* ev, ab_map_int inTest, ab_map_int inMember) /*i*/
{
	AB_USED_PARAMS_1(ev);
	return ( inTest == inMember );
}

char* ab_Env_AssocString(ab_Env* ev, ab_map_int inKey, ab_map_int inValue,
	char* outBuf, ab_num inHashBucket, ab_num inActualBucket) /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outBuf, "<ab_IntMap:assoc pair=\"#%lX:#%lX\" bucket=\"%lu->%lu\"/>",
		(long) inKey,             // pair=\"#%lX
		(long) inValue,           // :#%lX\"
		(long) inHashBucket,      // bucket=\"%lu
		(long) inActualBucket     // ->%lu\"
	);
#else
	*outBuf = 0; /* empty string */
	AB_USED_PARAMS_4(inKey,inValue,inHashBucket,inActualBucket);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outBuf;
}

// ````` ````` random import for large AB testing ````` ````` 

#ifdef AB_CONFIG_DEBUG

// we want mixed case here to provoke any potential mixed case bugs
static const char* RandAb_gAlpha = "aBcDeFgHiJkLmNoPqRsTuVwXyZ";

static void RandAb_ToAlpha(long seed, char* outNormal, char* outScrambled)
{
	outNormal[ 0 ] = outScrambled[ 5 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 1 ] = outScrambled[ 2 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 2 ] = outScrambled[ 4 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 3 ] = outScrambled[ 6 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 4 ] = outScrambled[ 7 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 5 ] = outScrambled[ 1 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 6 ] = outScrambled[ 0 ] = RandAb_gAlpha[ seed & 0x0F ];

	seed >>= 4;
	outNormal[ 7 ] = outScrambled[ 3 ] = RandAb_gAlpha[ seed & 0x0F ];

	outNormal[ 8 ] = outScrambled[ 8 ] = '\0';
}
#endif /*AB_CONFIG_DEBUG*/
// ===== ===== end random ldif code using IronDoc pseudo RNG ===== =====

static ab_num ab_Row_MakeRandomImportEntry(ab_Row* self,
	 ab_Env* ev, ab_num inSeed)
{
	char givenName[ 32 ];
	char surName[ 32 ];
	char fullName[ 64 ];
	char email[ 64 ];

	inSeed = (ab_num) AbI4_Random((long) inSeed);
	RandAb_ToAlpha((long) inSeed, surName, givenName);
	XP_SPRINTF(fullName, "%.16s %.16s", givenName, surName);
	XP_SPRINTF(email, "%.16s@netcape.com", givenName);
	
	if ( self->WriteCell(ev, fullName, AB_Column_kFullName) )
	{
		if ( self->WriteCell(ev, givenName, AB_Column_kGivenName) )
		{
			if ( self->WriteCell(ev, surName, AB_Column_kFamilyName) )
			{
				if ( self->WriteCell(ev, email, AB_Column_kEmail) )
				{
					// okay, done
				}
			}
		}
	}
	
	return inSeed;
}


#ifdef AB_CONFIG_DEBUG
	/*static*/
	ab_num ab_Store::MakeImportRandomSeed(ab_Env* ev) /*i*/
		// MakeImportRandomSeed makes a new, fresh random seed for import.
	{
		ab_num outSeed = 0;
		ab_Env_BeginMethod(ev, ab_Store_kClassName, "MakeImportRandomSeed")

		time_t now; time(&now);
		outSeed = now;
		
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("<ab_Store::MakeImportRandomSeed out=\"#%lX\"/>",
				(long) outSeed);
#endif /*AB_CONFIG_TRACE*/

		ab_Env_EndMethod(ev)
		return outSeed;
	}
#endif /*AB_CONFIG_DEBUG*/

#ifdef AB_CONFIG_DEBUG
	// we use a static init safe technique for initializing these globals,
	// and this involves *not* initializing them here (because static init
	// might wipe out earlier first init), so Purify might report uninitialized
	// read access, but this is correct, so leave it alone.
	static ab_num ab_Store_gSessionImportRandomSeed;
	static ab_num* ab_Store_gSessionSeedAddress;
#endif /*AB_CONFIG_DEBUG*/

#ifdef AB_CONFIG_DEBUG
	/*static*/
	ab_num* ab_Store::UseSessionImportRandomSeed(ab_Env* ev) /*i*/
	{
		ab_num* outSeedAddress = 0;
		ab_Env_BeginMethod(ev, ab_Store_kClassName,
			"UseSessionImportRandomSeed")
			
		outSeedAddress = ab_Store_gSessionSeedAddress;
		if ( outSeedAddress != &ab_Store_gSessionImportRandomSeed )
		{
			outSeedAddress = &ab_Store_gSessionImportRandomSeed;
			ab_Store_gSessionSeedAddress = outSeedAddress;
			*outSeedAddress = MakeImportRandomSeed(ev); // init first time
		}
		
	#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
			ev->Trace("<ab_Store::UseSessionImportRandomSeed out=\"#%lX\"/>",
				(long) *outSeedAddress);
	#endif /*AB_CONFIG_TRACE*/

		ab_Env_EndMethod(ev)
		return outSeedAddress;
	}
#endif /*AB_CONFIG_DEBUG*/

#define ab_Store_kMaxRandomAddCount 10000

#ifdef AB_CONFIG_DEBUG
void ab_Store::ImportRandomEntries(ab_Env* ev,
	 ab_num inCount, ab_num* ioRandomSeed) /*i*/
	// The random seed passed in ioRandomSeed is used to drive the content
	// of random entries added to the store, and a new seed representing the
	// continuation of this process is returned in ioRandomSeed.   This lets
	// multiple calls to ImportRandomEntries share the same random number
	// sequence to ensure duplicate random entries are not possible.  a good
	// strategy is to call MakeImportRandomSeed() once and save the result
	// in a global which is passed in all ImportRandomEntries() calls. (We
	// could do this internally, but we want to pass in the seed so the
	// process is deterministically repeatable if necessary.)
	//
	// A main reason to make many calls to ImportRandomEntries() with small
	// values for inCount (instead of one huge value for inCount) is to make
	// the system stay responsive, because ImportRandomEntries() might lock
	// up the current thread until all the inCount entries are added.
	//
	// For small memory footprints, and for dbs containing E entries, when
	// C new entries are added, elapsed time can be M minutes.  M can be as
	// bad as M = (C/1000)*5 + (E/1000)*5, or as good as M = (C/10000), but
	// the good time probably requires footprint exceeding added entries.
	// (The "bad" equation translates to "five minutes for each new thousand
	// added plus five minutes for each thousand in the db", and the "good"
	// equation says "one minute per ten thousand added.")  At the time of
	// this writing, expected time is about M = (C/1000)*2 + (E/1000)*2,
	// or two minutes per thousand added plus two minutes per db thousand.
	//
	// inCount is capped at a maximum of ten thousand to save your sanity.
{
	ab_Env_BeginMethod(ev, ab_Store_kClassName, "MakeImportRandomSeed")
	
	// ab_Store::UseGobsOfMemoryForAddressBookSpeed(ev);
	
	ab_num seed = (ioRandomSeed)? *ioRandomSeed: 1;
	if ( inCount > ab_Store_kMaxRandomAddCount )
		inCount = ab_Store_kMaxRandomAddCount;
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_Store::ImportRandomEntries cnt=\"%lu\" seed=\"#%lX\"/>",
			(long) inCount, (long) seed);
#endif /*AB_CONFIG_TRACE*/

	ab_Table* table = this->AcquireTopStoreTable(ev);
	if ( table )
	{
		ab_RowContent* content = table->TableRowContent();
		if ( content )
		{
	  		ab_cell_count cellCount = 10; // ten is safely more than we need
	  		
			ab_Row* row = new(*ev) ab_Row(ev, ab_Usage::kHeap, table, cellCount);
			if ( row )
			{
				if ( row->WriteCell(ev, "t", AB_Column_kIsPerson) )
				{
					this->BeginModelFlux(ev);
					
					++inCount; // set up for predecrement in loop test
					while ( ev->Good() && --inCount )
					{
						seed = ab_Row_MakeRandomImportEntry(row, ev, seed);
						if ( ev->Good() )
							content->NewRow(ev, row, /*inPos*/ 0);
					}
					
					this->SaveStoreContent(ev); // commit all the new rows

					this->EndModelFlux(ev);
				}
				
				row->ReleaseObject(ev); // always release when finished
			}
		}
		else ev->NewAbookFault(AB_Table_kFaultNullRowContentSlot);

		table->ReleaseObject(ev); // always release when done
	}
	
	if ( ioRandomSeed )
		*ioRandomSeed = seed;
	
	ab_Env_EndMethod(ev)
}
#endif /*AB_CONFIG_DEBUG*/

/*=============================================================================
 * ab_RowContent: basics for abstract row content interface
 */

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_RowContent_kClassName /*i*/ = "ab_RowContent";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

ab_RowContent::~ab_RowContent() /*i*/
{
	AB_ASSERT(mRowContent_Table==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mRowContent_Table;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_RowContent_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}
        
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab _RowContentmethods

ab_RowContent::ab_RowContent(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_Store* ioStore, ab_Table* ioTable)
	: ab_Part(ev, inUsage, ab_Session::GetGlobalSession()->NewTempRowUid(),
		ioStore)
{
	ab_Env_BeginMethod(ev, ab_RowContent_kClassName, ab_RowContent_kClassName)

	if ( ioTable )
	{
	  	if ( ioTable->AcquireObject(ev) )
	  		mRowContent_Table = ioTable; 
	}
	else ev->NewAbookFault(ab_RowContent_kFaultNullTable);
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

void ab_RowContent::CloseRowContent(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_RowContent_kClassName, "CloseRowContent")
	
	ab_Object* obj = mRowContent_Table;
	if ( obj )
	{
		mRowContent_Table = 0;
		obj->ReleaseObject(ev);
	}
	this->ClosePart(ev);

	ab_Env_EndMethod(ev)
}

ab_bool ab_RowContent::SetRowContentTable(ab_Env* ev, ab_Table* ioTable) /*i*/
{
	ab_Env_BeginMethod(ev, ab_RowContent_kClassName, "SetRowContentTable")
	
	if ( mRowContent_Table ) // need to release old table?
	{
		mRowContent_Table->ReleaseObject(ev);
		mRowContent_Table = 0;
	}
	if ( ev->Good() ) // no problems yet?
	{
		if ( ioTable )
		{
			if ( ioTable->AcquireObject(ev) ) // acquired new table?
				mRowContent_Table = ioTable;
		}
		else
		{
#ifdef AB_CONFIG_DEBUG
			ev->Break("<ab_RowContent:SetRowContentTable:null:new:table/>");
#endif /*AB_CONFIG_TRACE*/
		}
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

ab_row_pos ab_RowContent::FindRowInTableRowSet(ab_Env* ev, /*i*/
	ab_row_uid inRowUid)
	// calls FindRow() on mRowContent_Table->mTable_RowSet
{
	ab_row_pos outPos = 0;
	ab_Env_BeginMethod(ev, ab_RowContent_kClassName, "FindRowInTableRowSet")

	ab_Table* rowContentTable = this->GetRowContentTable();
	if ( rowContentTable && rowContentTable->IsOpenObject() )
	{
		ab_RowSet* rowSet = rowContentTable->TableRowSet();
		if ( rowSet && rowSet->IsOpenObject() )
		{
			outPos = rowSet->FindRow(ev, inRowUid);
		}
	}

	ab_Env_EndMethod(ev)
	return outPos;
}

/*=============================================================================
 * ab_RowSet: basics for abstract row set interface
 */
 
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_RowSet_kClassName /*i*/ = "ab_RowSet";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_RowSet methods


// called by ab_Table::TableSeesChangedStore():
void ab_RowSet::RowSetSeesChangedStore(ab_Env* ev, /*i*/
	 const ab_Change* inChange)
    // this default version notifies a scramble through mRowSet_Table
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "RowSetSeesChangedStore")

	if ( this->IsOpenOrClosingObject() )
	{
		// if we are being destroyed, then close instead of notify
		ab_row_uid rowSetUid = mPart_RowUid;
		ab_row_uid tableUid = mRowSet_Table->GetPartRowUid();
		ab_row_uid changeRowUid = inChange->mChange_Row;
		ab_bool isMe = ( changeRowUid == rowSetUid || changeRowUid == tableUid );
		
		ab_change_mask changeMask = inChange->mChange_Mask;
		ab_bool didKill = (( ab_Change_kKillRow & changeMask ) != 0);
		
		// also, if store is being closed, then we should close as well
		ab_bool storeClosing = (( ab_Change_kClosing & changeMask ) != 0);
		
		if ( storeClosing || ( didKill && isMe ) ) // closing or killed myself?
		{
			mRowSet_Table->CloseObject(ev);
		}
		else // otherwise just pass on the notification with the inherited method:
		{
			// make a new change to substitute this table's row uid:
			ab_Change change(*inChange, ab_Usage::kStack);
			change.mChange_ModelUid = mRowSet_Table->GetPartRowUid();
			change.mChange_Mask |= ab_Change_kScramble; // add scramble to mask
			mRowSet_Table->ChangedModel(ev, &change);
		}
	}

#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		ev->Trace("<ab_RowSet::RowSetSeesChangedStore $$ me=\"^%lX\" table=\"^%lX\" ch=\"^%lX\"/>",
			(long) this, (long) mRowSet_Table, (long) inChange);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

ab_row_count ab_RowSet::AddAllRows(ab_Env* ev,  /*i*/
	const ab_RowSet* inRowSet)
{
	ab_row_count outCount = 0;
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "AddAllRows")

	AB_USED_PARAMS_1(inRowSet);

  	ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);

	ab_Env_EndMethod(ev)
	return outCount;
}

/*virtual*/
void ab_RowSet::ChangeRowSetSortForward(ab_Env* ev, /*i*/
	ab_bool inForward)
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "ChangeRowSetSortForward")

	if ( mRowSet_SortForward != inForward )
	{
	  	mRowSet_SortForward = inForward;

		this->NotifyRowSetScramble(ev);
	}

	ab_Env_EndMethod(ev)
}


ab_row_uid ab_RowSet::FindRowByPrefix(ab_Env* ev, /*i*/
	const char* inPrefix, const ab_column_uid inCol)
{
	ab_row_uid outUid = 0;
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "FindRowByPrefix")

	AB_USED_PARAMS_2(inPrefix,inCol);
	
	ev->NewAbookFault(ab_RowSet_kFaultCannotSearchRows);

	ab_Env_EndMethod(ev)
	return outUid;
}

void ab_RowSet::NotifyRowSetScramble(ab_Env* ev) const /*i*/
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "NotifyRowSetScramble")
	
	if ( mRowSet_Table )
	{
		ab_Change c(ab_Usage::kStack, mRowSet_Table, ab_Change_kScramble);
		mRowSet_Table->ChangedModel(ev, &c);
	}
	else ev->NewAbookFault(ab_RowSet_kFaultNullTable);

	ab_Env_EndMethod(ev)
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

ab_RowSet::~ab_RowSet() /*i*/
{
	AB_ASSERT(mRowSet_Table==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mRowSet_Table;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_RowSet_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_RowSet methods

ab_RowSet::ab_RowSet(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_row_uid inRowUid, ab_Store* ioStore, ab_Table* ioTable)
	: ab_Part(ev, inUsage, inRowUid, ioStore),
	mRowSet_Table( 0 ),
	mRowSet_SortColUid( 0 ),
	mRowSet_SortDbUid( 0 ),
	mRowSet_SortForward( AB_kTrue )
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, ab_RowSet_kClassName)

	if ( ioTable )
	{
	  	if ( ioTable->AcquireObject(ev) )
	  		mRowSet_Table = ioTable; 
	}
	else ev->NewAbookFault(ab_RowSet_kFaultNullTable);
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
		this->TraceObject(ev);
#endif /*AB_CONFIG_TRACE*/

	ab_Env_EndMethod(ev)
}

void ab_RowSet::CloseRowSet(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "CloseRowSet")
	
	ab_Object* obj = mRowSet_Table;
	if ( obj )
	{
		mRowSet_Table = 0;
		obj->ReleaseObject(ev);
	}
	this->ClosePart(ev);

	ab_Env_EndMethod(ev)
}

ab_bool ab_RowSet::SetRowSetTable(ab_Env* ev, ab_Table* ioTable) /*i*/
{
	ab_Env_BeginMethod(ev, ab_RowSet_kClassName, "SetRowSetTable")
	
	if ( mRowSet_Table ) // need to release old table?
	{
		mRowSet_Table->ReleaseObject(ev);
		mRowSet_Table = 0;
	}
	if ( ev->Good() ) // no problems yet?
	{
		if ( ioTable )
		{
			if ( ioTable->AcquireObject(ev) ) // acquired new table?
				mRowSet_Table = ioTable;
		}
		else
		{
#ifdef AB_CONFIG_DEBUG
			ev->Break("<ab_RowSet:SetRowSetTable:null:new:table/>");
#endif /*AB_CONFIG_TRACE*/
		}
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

/*=============================================================================
 * file names
 */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
static const char* AB_FileName_kClassName /*i*/ = "AB_FileName";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

const char* AB_FileName_kCurrentSuffix = ".0a0"; // temp dev format
const char* AB_FileName_kFutureSuffix = ".na2"; // future native format

const char* AB_FileName_kImportSuffixArray[] = {
	".ldi", ".ldif",   // import LDIF
	".nab",            // import 4.0.x
	".0a0",            // import current format
	(const char*) 0
};

// later we'll support html as well:
// ".htm", ".html", 

AB_API_IMPL(AB_Store_eType) /* abstore.cpp */
AB_FileName_GetStoreType(const char* inFileName, AB_Env* cev) /*i*/
	/*- GetStoreType() returns an enum value that describes how the file
	can be handled.  Currently this is based on the file name itself, but
	later this might also involve looking inside the file to examine the
	bytes to determine correct usage.  The file can only be opened and
	used as an instance of AB_Store (using AB_Env_NewStore()) if the type
	equals AB_StoreType_kTable, which means the store supports the table
	interface.  Otherwise, if the file type is AB_StoreType_kImport, this
	means one should create a new database with an appropriate name (see
	AB_FileName_MakeNativeName()), and import this file.  (Or the new file
	might already exist from earlier, openable with AB_Env_NewStore().)
	Formats which cannot opened nor imported might be either "unknown"
	or "stale" (AB_StoreType_kUnknown or AB_StoreType_kStale), where the
	stale formats are recognizably retired transient development formats,
	while unknown formats might still be importable by other means.  When
	a "future" format is detected which has not yet been implemented, the
	type returned is AB_StoreType_kFuture, and presumably this means that
	neither opening nor importing is feasible (like stale formats) but
	that products with later versions should read the format.
	
	The native format will be ".na2", but we can use transient development
	formats before the final format with different suffixes like ".na0".
	Later a ".na0" format will return AB_StoreType_kStale.
	
	The imported formats will include ".ldi" and ".ldif" for LDIF, with
	".htm" and ".html" for HTML.  Later we'll support more formats.
	
	The unknown file formats might still be importable using more general
	purpose import code that can recognize a broader range of formats. -*/
{
	AB_Store_eType outType = AB_StoreType_kUnknown;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_FileName_kClassName, "GetStoreType")
	
	if ( inFileName )
	{
		const char* suffix = AB_FileName_FindSuffix(inFileName);
		if ( suffix )
		{
			if ( AB_STRCMP(suffix, AB_FileName_kCurrentSuffix) == 0 )
			{
				outType = AB_StoreType_kTable;
			}
			else if ( AB_STRCMP(suffix, AB_FileName_kFutureSuffix) == 0 )
			{
				outType = AB_StoreType_kFuture;
			}
			else
			{
				const char** array = AB_FileName_kImportSuffixArray;
				while ( *array )
				{
					if ( AB_STRCMP(suffix, *array) == 0 )
					{
						outType = AB_StoreType_kImport;
						break;
					}
					++array;
				}
			}
		}
	}
	
	ab_Env_EndMethod(ev)
	return outType;
}

AB_API_IMPL(void) /* abstore.cpp */
AB_FileName_GetNativeSuffix(char* outFileNameSuffix8, AB_Env* cev) /*i*/
	/*- GetNativeSuffix() writes into outFileNameSuffix8 the file name 
	extension suffix (for example, ".na2") that is preferred for the
	current native database format created for new database files.  The
	space should be at least eight characters in length for safety, though
	in practice five bytes will be written: a period, followed by three
	alphanumeric characters, followed by a null byte.  Whenever one wishes
	to make a new database, the filename for the database should end with
	this string suffix.  (Note that implementations might return more than
	one string suffix, assuming the environment has access to prefs or some
	other indication of which format to choose among possibilities.) -*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_FileName_kClassName, "GetNativeSuffix")
	
	if ( outFileNameSuffix8 )
	{
		AB_STRCPY(outFileNameSuffix8, AB_FileName_kCurrentSuffix);
	}
	
	ab_Env_EndMethod(ev)
}

AB_API_IMPL(ab_bool) /* abstore.cpp */
AB_FileName_HasNativeSuffix(const char* inFileName, AB_Env* cev) /*i*/
	/*- HasNativeSuffix() returns true only if inFileName ends with a
	suffix equal to that returned by AB_FileName_GetNativeSuffix(). -*/
{
	ab_bool outBool = AB_kFalse;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_FileName_kClassName, "HasNativeSuffix")
	
	if ( inFileName )
	{
		const char* suffix = AB_FileName_FindSuffix(inFileName);
		if ( suffix )
		{
			outBool = ( AB_STRCMP(suffix, AB_FileName_kCurrentSuffix) == 0 );
		}
	}
	
	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(const char*) /* abstore.cpp */
AB_FileName_FindSuffix(const char* inFileName) /*i*/
	/*- FindSuffix() returns the address of the last period "." in the
	string inFileName, or null if no period is found at all.  This method
	is used to locate the suffix for comparison in HasNativeSuffix(). 
	Typically file format judgments will be made on the three characters
	that follow the final period in the filename. -*/
{
	const char* outSuffix = 0;
	--inFileName; // prepare for preincrement:
	
	while ( *++inFileName ) // another byte in string?
	{
		if ( *inFileName == '.' ) // found another period?
			outSuffix = inFileName; // replace previous last period address
	}
	return outSuffix;
}

AB_API_IMPL(char*) /* abstore.cpp */
AB_FileName_MakeNativeName(const char* inFileName, AB_Env* cev) /*i*/
	/*- MakeNativeName() allocates a new string (which must be freed
	later using XP_FREE()) for which AB_FileName_HasNativeSuffix()
	is true.  If inFileName already has the native suffix, the string
	returned is a simple copy; otherwise the returned string appends
	the value of AB_FileName_GetNativeSuffix() to the end. (Note that
	the environment might select one of several format choices.) -*/
{
	char* outName = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_FileName_kClassName, "MakeNativeName")
	
	if ( !inFileName || !*inFileName ) // null or empty?
		inFileName = "null";
	
	if ( AB_FileName_HasNativeSuffix(inFileName, cev) ) // already fine?
	{
		outName = ev->CopyString(inFileName); // just copy it
	}
	else // concat the current suffix on the end of inFileName
	{
	  	ab_num length = AB_STRLEN(inFileName);
			
	  	outName = (char*) XP_ALLOC(length + 8); // plus more space for suffix
	  	if ( outName ) // alloc succeeded?
		{
		   	AB_STRCPY(outName, inFileName); // copy
		   	AB_STRCPY(outName + length, AB_FileName_kCurrentSuffix); // concat
		}
		else ev->NewAbookFault(AB_Env_kFaultOutOfMemory);
	}
	
	ab_Env_EndMethod(ev)
	return outName;
}

/*=============================================================================
 * ab_Session
 */


// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Session methods

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
static const char* ab_Session_kClassName /*i*/ = "ab_Session";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

ab_Session::ab_Session() /*i*/
	// initialization is separated from construction so platforms
	// with poor static construction support will not be a problem.
{
}

void ab_Session::InitSession() /*i*/
{
	mSession_Tag = ab_Session_kTag;
	mSession_GlobalTable = 0;
	mSession_TempRowSeed = 1;
	mSession_TempChangeSeed = 1;
	mSession_DefaultSortColumn = AB_Column_kFullName;
}

static ab_Session ab_Session_g_instance; // note: static init might not occur
static ab_Session* ab_Session_g_pointer = 0; // nonzero only after init

/*static*/
ab_Session* ab_Session::GetGlobalSession() /*i*/
{
	if ( !ab_Session_g_pointer )
	{
		ab_Session_g_instance.InitSession();
		ab_Session_g_pointer = &ab_Session_g_instance;
	}
	return ab_Session_g_pointer;
}

void ab_Session::SetDefaultSortColumn(ab_Env* ev, /*i*/
	ab_column_uid newSortCol) 
{
	ab_Env_BeginMethod(ev, ab_Session_kClassName, "SetDefaultSortColumn")
	
#ifdef AB_CONFIG_TRACE
	if ( ev->DoTrace() )
	{
		ev->Trace(
			"<ab_Session::SetDefaultSortColumn from=\"#%08LX\" to=\"#%08LX\"/>",
			(long) mSession_DefaultSortColumn,
			(long) newSortCol);
	}
#endif /*AB_CONFIG_TRACE*/

	if ( AB_Uid_IsStandardColumn(newSortCol) )
	{
		mSession_DefaultSortColumn = newSortCol;
	}
	else ev->NewAbookFault(AB_Column_kFaultNotStandardUid);

	ab_Env_EndMethod(ev)
}

/*static*/
void ab_Store::DestroyStoreWithFileName(ab_Env* ev, const char* inFileName)
	// DestroyStoreWithFileName() deletes the file from the file system
	// containing the store that would be opened if a store instance was
	// created using inFileName for the file name. This method is provided
	// to keep file destruction equally as abstract as the implicit file
	// creation occuring when a new store is created.  Otherwise it would
	// be difficult to discard databases as easily as they are created. But
	// this method is dangerous in the sense that destruction of data is
	// definitely intended, so don't call this method unless the total
	// destruction of data in the store is what you have in mind.  This
	// method does not attempt to verify the file contains a database before
	// destroying the file with name inFileName in whatever directory is
	// used to host the database files.  Make sure you name the right file.
{
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "DestroyStoreWithFileName")

	if ( inFileName && *inFileName )
	{
#ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			ev->Trace( "<ab_Store::DestroyStoreWithFileName name=\"%.96s\"/>",
				inFileName);
		}
#endif /*AB_CONFIG_TRACE*/

		XP_FileRemove(inFileName, xpAddrBook);
	}

	ab_Env_EndMethod(ev)
}

AB_API_IMPL(void) /* abstore.cpp */
AB_Env_DestroyStoreWithFileName(AB_Env* self, const char* inFileName) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(self);
	ab_Env_BeginMethod(ev, "AB_Env", "DestroyStoreWithFileName")
	
	ab_Store::DestroyStoreWithFileName(ev, inFileName);
	
	ab_Env_EndMethod(ev)
}

AB_API_IMPL(AB_Store*) /* abstore.cpp */
AB_Env_NewStore(AB_Env* self, const char* inFileName, /*i*/
	ab_num inTargetFootprint)
{
	AB_Store* outStore = 0;
	ab_Env* ev = ab_Env::AsThis(self);
	ab_Env_BeginMethod(ev, "AB_Env", "NewStore")

	AB_Store_eType type = AB_FileName_GetStoreType(inFileName, self);
	switch ( type )
	{
		case AB_StoreType_kUnknown: // 0
			ev->NewAbookFault(ab_Store_kFaultUnknownStoreFormat);
			break;
			
		case AB_StoreType_kTable: // 1
			outStore = (AB_Store*)
				ab_Factory::MakeStore(ev, inFileName, inTargetFootprint);
			break;
			
		case AB_StoreType_kImport: // 2
			ev->NewAbookFault(ab_Store_kFaultImportFormatOnly);
			break;
			
		case AB_StoreType_kStale: // 3
			ev->NewAbookFault(ab_Store_kFaultStaleStoreFormat);
			break;
			
		case AB_StoreType_kFuture: // 4
			ev->NewAbookFault(ab_Store_kFaultFutureStoreFormat);
			break;
			
		default: // ?
			ev->NewAbookFault(ab_Store_kFaultUnknownStoreFormat);
			break;
			
	}
	
	ab_Env_EndMethod(ev)
	return outStore;
}

/* ````` opening store CONTENT ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_OpenStoreContent(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "OpenStoreContent")

	((ab_Store*) self)->OpenStoreContent(ev);
	
	ab_Env_EndMethod(ev)
}

/* ````` closing store CONTENT (aborts unsaved changes) ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_CloseStoreContent(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "CloseStoreContent")

	((ab_Store*) self)->CloseStoreContent(ev);
	
	ab_Env_EndMethod(ev)
}

/* ````` saving store CONTENT (commits unsaved changes) ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_SaveStoreContent(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "SaveStoreContent")

	((ab_Store*) self)->SaveStoreContent(ev);
	
	ab_Env_EndMethod(ev)
}

/* ````` importing store CONTENT ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_ImportWithPromptForFileName(AB_Store* self, AB_Env* cev, /*i*/ 
	MWContext* ioContext)
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "ImportWithPromptForFileName")

	((ab_Store*) self)->ImportWithPromptForFileName(ev, ioContext);
	
	ab_Env_EndMethod(ev)
}

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_ImportEntireFileByName(AB_Store * self, AB_Env* cev, /*i*/
	 const char* inFileName)
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "ImportEntireFileByName")

	((ab_Store*) self)->ImportEntireFileByName(ev, inFileName);

	ab_Env_EndMethod(ev)
}

/* ````` exporting store CONTENT ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_ExportWithPromptForFileName(AB_Store* self, AB_Env* cev, /*i*/ 
	MWContext* ioContext)
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "ExportWithPromptForFileName")

	((ab_Store*) self)->ExportWithPromptForFileName(ev, ioContext);
	
	ab_Env_EndMethod(ev)
}

/* ````` batching changes (these calls should nest okay) ````` */

AB_API_IMPL(void) /* abstore.cpp, commit every inEventThreshold changes */
AB_Store_StartBatchMode(AB_Store* self, AB_Env* cev, /*i*/
	ab_num inEventThreshold)
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "StartBatchMode")

	((ab_Store*) self)->StartBatchMode(ev, inEventThreshold);
	
	ab_Env_EndMethod(ev)
}

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_EndBatchMode(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "EndBatchMode")

	((ab_Store*) self)->EndBatchMode(ev);
	
	ab_Env_EndMethod(ev)
}

/* ````` open/close content status querying ````` */

AB_API_IMPL(ab_bool) /* abstore.cpp */
AB_Store_IsOpenStoreContent(AB_Store* self) /*i*/
{
	return ((ab_Store*) self)->IsOpenStoreContent();
}

AB_API_IMPL(ab_bool) /* abstore.cpp */
AB_Store_IsShutStoreContent(AB_Store* self) /*i*/
{
	return ((ab_Store*) self)->IsShutStoreContent();
}
	
/* ````` closing store INSTANCE ````` */

AB_API_IMPL(void) /* abstore.cpp */
AB_Store_CloseObject(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "CloseObject")

	((ab_Store*) self)->CloseObject(ev);
	
	ab_Env_EndMethod(ev)
}
	
/* ````` store refcounting ````` */

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_Store_Acquire(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "Acquire")

	outCount = ((ab_Store*) self)->AcquireObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_Store_Release(AB_Store* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "Release")

	outCount = ((ab_Store*) self)->ReleaseObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

/* ````` accessing top store table (with or without acquire)  ````` */

AB_API_IMPL(AB_Table*) /* abstore.cpp */
AB_Store_GetTopStoreTable(AB_Store* self, AB_Env* cev) /*i*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "GetTopStoreTable")

	outTable = (AB_Table*) ((ab_Store*) self)->GetTopStoreTable(ev);
	
	ab_Env_EndMethod(ev)
	return outTable;
}

AB_API_IMPL(AB_Table*) /* abstore.cpp */
AB_Store_AcquireTopStoreTable(AB_Store* self, AB_Env* cev) /*i*/
{
	AB_Table* outTable = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "AcquireTopStoreTable")

	outTable = (AB_Table*) ((ab_Store*) self)->AcquireTopStoreTable(ev);
	
	ab_Env_EndMethod(ev)
	return outTable;
}


/* ````` file-and-thumb based import and export ````` */

AB_API_IMPL(AB_File*) /* abstore.cpp */
AB_Store_NewImportFile(AB_Store* self, AB_Env* cev, const char* inFileName) /*i*/
{
	AB_File* outFile = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "NewImportFile")

	if ( inFileName && *inFileName )
	{
		XP_File fp = 0;
		ab_u4 formatHint = ab_File_kUnknownFormat;
		
		const char* suffix = AB_FileName_FindSuffix(inFileName);
		if ( AB_STRNCMP(suffix, ".ldi", 4) == 0 )
		{
			fp = XP_FileOpen(inFileName, xpLDIFFile, XP_FILE_READ_BIN);
			if ( fp )
				formatHint = ab_File_kLdifFormat;
		}
		else if ( AB_STRNCMP(suffix, ".htm", 4) == 0 )
		{
			fp = XP_FileOpen(inFileName, xpBookmarks, XP_FILE_READ);
			if ( fp )
				formatHint = ab_File_kHtmlFormat;
		}
		else if ( AB_STRCMP(suffix, AB_FileName_kCurrentSuffix) == 0 )
		{
			ab_File* file = ab_Factory::MakeOldFile(ev, inFileName);
			if ( file )
			{
				if ( ev->Good() )
					outFile = (AB_File*) file;
				else
					file->ReleaseObject(ev);
			}
		}
		else if ( AB_STRCMP(suffix, ".nab") == 0 )
		{
			ab_File* file = ab_Factory::MakeNewFile(ev, inFileName);
			if ( file )
			{
				if ( ev->Good() )
					outFile = (AB_File*) file;
				else
					file->ReleaseObject(ev);
			}
		}
		else if ( AB_STRCMP(suffix, AB_FileName_kFutureSuffix) == 0 )
		{
			ev->NewAbookFault(ab_Store_kFaultFutureStoreFormat);
		}
		else
			ev->NewAbookFault(ab_Store_kFaultUnknownImportFormat);
			
		if ( fp ) // ldif or html xp file opened?
		{
			ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
				fp, inFileName, /*inFrozen*/ AB_kTrue);
			if ( file )
			{
				if ( ev->Good() )
				{
					outFile = (AB_File*) file;
					file->SetFileIoOpen(AB_kTrue); // must eventually close
					file->ChangeFileFormatHint(formatHint);
				}
				else
					file->ReleaseObject(ev);
			}
			if ( !outFile )
				XP_FileClose(fp);
		}
		
	}
	else ev->NewAbookFault(ab_File_kFaultNoFileName);
	
	ab_Env_EndMethod(ev)
	return outFile;
}

AB_API_IMPL(ab_bool) /* abstore.cpp, true iff !AB_Thumb_IsProgressFinished() */
AB_Store_ContinueImport(AB_Store* self, AB_Env* cev, AB_File* ioFile, /*i*/
	 AB_Thumb* ioThumb)
{
	ab_Thumb* thumb = (ab_Thumb*) ioThumb;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "ContinueImport")

	if ( thumb->mThumb_FileFormat == AB_File_kLdifFormat )
		((ab_Store*) self)->ImportLdif(ev, ((ab_File*) ioFile), thumb);
	else if ( thumb->mThumb_FileFormat == AB_File_kBinaryFormat )
		((ab_Store*) self)->ImportOldBinary(ev, ((ab_File*) ioFile), thumb);
	else if ( thumb->mThumb_FileFormat == AB_File_kNativeFormat )
		((ab_Store*) self)->ImportCurrentNative(ev, ((ab_File*) ioFile), thumb);
	else
		ev->NewAbookFault(ab_Store_kFaultUnknownImportFormat);
	
	ab_Env_EndMethod(ev)
	return !thumb->IsProgressFinished();
}

AB_API_IMPL(AB_File*) /* abstore.cpp */
AB_Store_NewExportFile(AB_Store* self, AB_Env* cev,
	const char* inFileName)
{
	AB_File* outFile = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "NewImportFile")

	if ( inFileName && *inFileName )
	{
		XP_File fp = XP_FileOpen(inFileName, xpLDIFFile, XP_FILE_WRITE_BIN);
		if ( fp )
		{
			ab_StdioFile* file = new(*ev) ab_StdioFile(ev, ab_Usage::kHeap,
				fp, inFileName, /*inFrozen*/ AB_kFalse);
			if ( file )
			{
				if ( ev->Good() )
				{
					outFile = (AB_File*) file;
					file->SetFileIoOpen(AB_kTrue); // must eventually close xp file
				}
				else
					file->ReleaseObject(ev);
			}
			if ( !outFile )
				XP_FileClose(fp);
		}
	}
	else ev->NewAbookFault(ab_File_kFaultNoFileName);
	
	ab_Env_EndMethod(ev)
	return outFile;
}

AB_API_IMPL(ab_bool) /* abstore.cpp, true iff !AB_Thumb_IsProgressFinished() */
AB_Store_ContinueExport(AB_Store* self, AB_Env* cev, AB_File* ioFile, /*i*/
	 AB_Thumb* ioThumb)
{
	ab_Thumb* thumb = (ab_Thumb*) ioThumb;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Store_kClassName, "ContinueImport")

	((ab_Store*) self)->ExportLdif(ev, ((ab_File*) ioFile), thumb);
	
	ab_Env_EndMethod(ev)
	return !thumb->IsProgressFinished();
}
