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

/* file: abthumb.cpp
** Some portions derive from public domain IronDoc code and interfaces.
**
** Changes:
** <0> 09Mar1998 first implementation
*/

#ifndef _ABTABLE_
#include "abtable.h"
#endif

#ifndef _ABMODEL_
#include "abmodel.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Thumb_kClassName /*i*/ = "ab_Thumb";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* AB_Thumb_kClassName /*i*/ = "AB_Thumb";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

/*=============================================================================
 * ab_Thumb: import or export position
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Thumb::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	const char* conflictsName = mThumb_ConflictReportFileName;
	if ( !conflictsName )
		conflictsName = "";
		
	sprintf(outXmlBuf,
		"<ab_Thumb:str me=\"^%lX\" map=\"^%lX\" fp=\"#%lX\" rp=\"%lu\" rl=\"%lu\" bl=\"%lu\" cf=\"%.16s/^%lX\" form=\"%.4s\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                      // me=\"^%lX\"
		(long) mThumb_ListPosMap,         // map=\"^%lX\"

		(long) mThumb_FilePos,            // fp=\"#%lX\"
		(long) mThumb_RowPos,             // rp=\"%lu\"
		(long) mThumb_RowCountLimit,      // rl=\"%lu\"
		(long) mThumb_ByteCountLimit,     // bl=\"%lu\"
		
		conflictsName,                    // cf=\"%.16s
		(long) mThumb_ReportFile,         // /^%lX\"
	
		(const char*) &mThumb_FileFormat, // form=\"%.4s\"
		
		(unsigned long) mObject_RefCount,    // rc=\"%lu\"
		this->GetObjectAccessAsString(),     // ac=\"%.9s\"
		this->GetObjectUsageAsString()       // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_Thumb::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseThumb(ev);
		this->MarkShut();
	}
}

void ab_Thumb::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab_Thumb>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PushDepth(ev); // indent all objects in the list

		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		if ( mThumb_ListPosMap )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mThumb_ListPosMap->PrintObject(ev, ioPrinter);
		}
		if ( mThumb_ReportFile )
		{
			ioPrinter->NewlineIndent(ev, /*count*/ 1);
			mThumb_ReportFile->PrintObject(ev, ioPrinter);
		}
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab_Thumb>");
#else /*AB_CONFIG_PRINT*/
	AB_USED_PARAMS_2(ev, ioPrinter);
#endif /*AB_CONFIG_PRINT*/
}

ab_Thumb::~ab_Thumb() /*i*/
{
	AB_ASSERT(mThumb_ListPosMap==0);
	AB_ASSERT(mThumb_ReportFile==0);
	AB_ASSERT(mThumb_ConflictReportFileName==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	ab_Object* obj = mThumb_ListPosMap;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Thumb_kClassName);
		
	obj = mThumb_ReportFile;
	if ( obj )
		obj->ObjectNotReleasedPanic(ab_Thumb_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}
    
// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Thumb methods

ab_Thumb::ab_Thumb(ab_Env* ev, const ab_Usage& inUsage) /*i*/
	: ab_Object(inUsage)
,	mThumb_ListPosMap( 0 )

,	mThumb_ImportConflictPolicy( 0 )
,	mThumb_ConflictReportFileName( 0 )
,	mThumb_ReportFile( 0 )

,	mThumb_FilePos( 0 )
,	mThumb_RowPos( 0 )
,	mThumb_RowCountLimit( 0xFFFFFFFF ) // very large ("all the rows")
,	mThumb_ByteCountLimit( 0xFFFFFFFF ) // very large ("all the file bytes")

,   mThumb_ImportFileLength( 0 ) // total file bytes to be imported
,   mThumb_ExportTotalRowCount( 0 ) // total rows to be exported

,	mThumb_CurrentPass( 0 ) // have not started first pass yet
,	mThumb_FileFormat( ab_File_kLdifFormat )
	
,   mThumb_ListCountInFirstPass( 0 )
,   mThumb_ListCountInSecondPass( 0 )

,   mThumb_PortingCallCount( 0 ) // times ImportLdif() or ExportLdif() is called
,   mThumb_HavePreparedSecondPass( AB_kFalse )
{
	AB_USED_PARAMS_1(ev);
}

static const char*
ab_Thumb_kDefaultConflictReportFileName = "ab.import.conflicts";

void ab_Thumb::determine_import_conflict_policy(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Thumb_kClassName,
		"determine_import_conflict_policy")
	
	if ( !mThumb_ConflictReportFileName )
	{
		ab_policy policy = (ab_policy) AB_Policy_ImportConflicts_kDefault;
		char nameBuf[ AB_Env_kConflictReportFileNameSize ];
		const char* name = ab_Thumb_kDefaultConflictReportFileName;
		AB_Env* cev = ev->AsSelf();
		AB_Env_mImportConflictPolicy fun = cev->sEnv_ImportConflictPolicy;
		if ( fun )
		{
			policy = (*fun)(cev, nameBuf);
			nameBuf[ AB_Env_kConflictReportFileNameSize - 1 ] = 0; // force end
			if ( ev->Good() )
				name = nameBuf;
				
			if ( policy > (ab_policy) AB_Policy_ImportConflicts_kMax )
				policy = (ab_policy) AB_Policy_ImportConflicts_kDefault;
		}
		mThumb_ImportConflictPolicy = policy;
		mThumb_ConflictReportFileName = ev->CopyString(name);
	}
	
	ab_Env_EndMethod(ev)
}
    	
ab_policy ab_Thumb::GetImportConflictPolicy(ab_Env* ev) /*i*/
{
	ab_policy outPolicy = mThumb_ImportConflictPolicy;
	if ( !outPolicy )
	{
		ab_Env_BeginMethod(ev, ab_Thumb_kClassName, "GetImportConflictPolicy")
		
		this->determine_import_conflict_policy(ev);
		outPolicy = mThumb_ImportConflictPolicy;
		if ( !outPolicy )
			outPolicy = AB_Policy_ImportConflicts_kDefault;

		ab_Env_EndMethod(ev)
	}
	return outPolicy;
}

const char* ab_Thumb::GetConflictReportFileName(ab_Env* ev) /*i*/
{
	const char* outName = mThumb_ConflictReportFileName;
	if ( !outName )
	{
		ab_Env_BeginMethod(ev, ab_Thumb_kClassName, "GetConflictReportFileName")
		
		this->determine_import_conflict_policy(ev);
		outName = mThumb_ConflictReportFileName;
		if ( !outName )
			outName = ab_Thumb_kDefaultConflictReportFileName;
		
		ab_Env_EndMethod(ev)
	}
	return outName;
}

ab_StdioFile* ab_Thumb::GetReportFile(ab_Env* ev) /*i*/
{
	ab_StdioFile* outFile = mThumb_ReportFile;
	if ( !outFile )
	{
		ab_Env_BeginMethod(ev, ab_Thumb_kClassName, "GetReportFile")
		
		const char* fileName = this->GetConflictReportFileName(ev);
		if ( fileName && ev->Good() )
		{
			ab_StdioFile* file = new(*ev)
				ab_StdioFile(ev, ab_Usage::kHeap, fileName, "a+");
			if ( file )
			{
				if ( ev->Good() )
					mThumb_ReportFile = file;
				else
					file->ReleaseObject(ev);
			}
		}
		
		outFile = mThumb_ReportFile;
		ab_Env_EndMethod(ev)
	}
	return outFile;
}

ab_IntMap* ab_Thumb::GetListPosMap(ab_Env* ev) /*i*/
	// Get the hash table that maps list uids to file positions.
	// This method creates this object lazily on the first request, and
	// then keeps the map until the thumb is closed.  If one wishes to
	// know whether there is any content without forcing this table to
	// exist, one can call GetMappedListUidCount() to get the number of
	// lists that have been mapped, which can avoid creating the map.
{
	ab_IntMap* outIntMap = mThumb_ListPosMap;
	if ( !outIntMap )
	{
		ab_Env_BeginMethod(ev, ab_Thumb_kClassName, "GetListPosMap")
		
		ab_IntMap* map = new(*ev)
			ab_IntMap(ev, ab_Usage::kHeap, /*inKeyMethods*/ 0,
			ab_IntMap_kMailingListImportHint);
		
		if ( map )
		{
			if ( ev->Good() )
				mThumb_ListPosMap = outIntMap = map;
			else
				map->ReleaseObject(ev);
		}
		
		ab_Env_EndMethod(ev)
	}
	return outIntMap;
}

ab_num ab_Thumb::PickHeuristicStreamBufferSize() const /*i*/
	// Typically returns ab_Thumb_kNormalSteamBufSize except when values
	// of either (or both) mThumb_RowCountLimit and mThumb_ByteCountLimit
	// imply substantially more or less buffer space might make more sense.
	// Bounded by kMin and kMax values defined near kNormalSteamBufSize.
	// For sizing purposes, average row size is assumed kAverageRowSize.
{
	ab_num outSize = ab_Thumb_kNormalSteamBufSize; // default value
	
	// pick larger of expected bytes or expected row record size
	ab_num expected = mThumb_ByteCountLimit;
	
	ab_num rows = mThumb_RowCountLimit;
	if ( rows < ab_Thumb_kMaxSteamBufSize ) // multiply if not already huge
		rows *= ab_Thumb_kAverageRowSize;
	
	if ( rows > expected )
		expected = rows;
		
	if ( expected < outSize ) // seem to need less buffer space?
	{
		ab_num diff = outSize - expected;
		if ( diff > 1024 ) // difference seems sizeable?
		{
			outSize = expected + 1024; // allow for a bit extra
			if ( outSize < ab_Thumb_kMinSteamBufSize ) // below min?
				outSize = ab_Thumb_kMinSteamBufSize; // no less than this
		}   
	}
	else if ( expected > outSize ) // seem to need more buffer space?
	{
		if ( expected > ab_Thumb_kMaxSteamBufSize )
			outSize = ab_Thumb_kMaxSteamBufSize; // no more than this
		else
		{
			outSize = expected + 1024; // allow for a bit extra
			if ( outSize > ab_Thumb_kMaxSteamBufSize ) // above max?
				outSize = ab_Thumb_kMaxSteamBufSize; // no more than this
		}
	}
	return outSize;
}


void ab_Thumb::CloseThumb(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Thumb_kClassName, "CloseThumb")

	ab_Object* obj = mThumb_ListPosMap;
  	if ( obj )
  	{
  		mThumb_ListPosMap = 0;
  		obj->ReleaseObject(ev);
  	}
  	
  	obj = mThumb_ReportFile;
  	if ( obj )
  	{
  		if ( mThumb_ReportFile->IsOpenAndActiveFile() )
  			mThumb_ReportFile->Flush(ev);
  			
  		mThumb_ReportFile->CloseStdio(ev);
  		
  		mThumb_ReportFile = 0;
  		obj->ReleaseObject(ev);
  	}
  	
  	char* name = mThumb_ConflictReportFileName;
  	if ( name )
  	{
  		mThumb_ConflictReportFileName = 0;
  		ev->FreeString(name);
  	}

	ab_Env_EndMethod(ev)
}


/*=============================================================================
 * ab_IntMap: collection of assocs from key uid to value uid 
 */

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_IntMap_kClassName /*i*/ = "ab_IntMap";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_IntMapIter_kClassName /*i*/ = "ab_IntMapIter";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // virtual ab_Object methods

char* ab_IntMap::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	XP_SPRINTF(outXmlBuf,
		"<ab_IntMap:str me=\"^%lX\" chg=\"%lu\" cap:thr=\"%lu:%lu\" cnt=\"%lu\" k:v=\"^%lX:^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                             // me=\"^%lX\"
		(long) mIntMap_ChangeCount,              // chg=\"%lu\"
		(long) mIntMap_Capacity,                 // cap:thr=\"%lu
		(long) mIntMap_Threshold,                // :%lu\"
		(long) mIntMap_AssocCount,               // cnt=\"%lu\"
		(long) mIntMap_KeyInts,                  // k:v=\"^%lX
		(long) mIntMap_ValueInts,                // :^%lX\"
		
		(unsigned long) mObject_RefCount,        // rc=\"%lu\"
		this->GetObjectAccessAsString(),         // ac=\"%.9s\"
		this->GetObjectUsageAsString()           // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}

void ab_IntMap::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseIntMap(ev);
		this->MarkShut();
	}
}

void ab_IntMap::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab_IntMap>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		
		ab_map_int* keys = mIntMap_KeyInts;
		if ( keys ) // any associations?
		{
			ab_map_int* end = keys + mIntMap_Capacity; // one past last bucket
			
			--keys; // prepare for preincrement

			ioPrinter->PushDepth(ev); // indent all buckets
			
			ab_error_count ec = ev->ErrorCount();
			while ( ++keys < end && ec <= ev->ErrorCount() )
			{
				ab_map_int k = *keys;
				if ( k ) // is this bucket in use?
				{
					ab_u4 actual = keys - mIntMap_KeyInts;
					ab_u4 hash = this->hash_bucket(ev, k);

					if ( mIntMap_ValueInts ) // print the value too?
					{
						ab_map_int v = mIntMap_ValueInts[ actual ];
						(*mIntMap_AssocString)(ev, k, v, xmlBuf, hash, actual);
						
						//XP_SPRINTF(xmlBuf,
						//"<ab_IntMap:assoc pair=\"#%lX:#%lX\" bucket=\"%lu->%lu\"/>",
						//	(long) k,             // pair=\"#%lX
						//	(long) v,             // :#%lX\"
						//	(long) hashBucket,    // bucket=\"%lu
						//	(long) actualBucket   // ->%lu\"
						//);
					}
					else // each bucket contains only a key
					{
						ab_map_int v = ab_IntMap_kPretendValue;
						(*mIntMap_AssocString)(ev, k, v, xmlBuf, hash, actual);
						//XP_SPRINTF(xmlBuf,
						//"<ab_IntMap:elem key=\"#%lX\" bucket=\"%lu->%lu\"/>",
						//	(long) k,             // key=\"#%lX\"
						//	(long) hashBucket,  // bucket=\"%lu
						//	(long) actualBucket   // ->%lu\"
						//);
					}
					ioPrinter->NewlineIndent(ev, /*count*/ 1);
					ioPrinter->PutString(ev, xmlBuf);
				}
			}
			ioPrinter->PopDepth(ev); // stop bucket indentation
		}
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab_IntMap>");
#else /*AB_CONFIG_PRINT*/
	AB_USED_PARAMS_2(ev,ioPrinter);
#endif /*AB_CONFIG_PRINT*/
}

ab_IntMap::~ab_IntMap() /*i*/
{
	AB_ASSERT(mIntMap_KeyInts==0);
	AB_ASSERT(mIntMap_ValueInts==0);
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// private: // private non-poly ab_IntMap helper methods

void ab_IntMap::grow_int_map(ab_Env* ev) /*i*/
	// increase capacity by some percentage
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "grow_int_map")

  	if ( mIntMap_AssocCount >= mIntMap_Threshold )
  	{
  		ab_num oldThreshold = mIntMap_Threshold; // save before we modify 
  		ab_num newSizeHint = ( mIntMap_Threshold * 4 ) / 3; // 33% growth
  		mIntMap_Threshold = newSizeHint; // temporarily bigger during copy:
  		
  		ab_IntMap biggerCopy(ev, ab_Usage::kStack, newSizeHint, *this);
  		mIntMap_Threshold = oldThreshold; // restore original value
 
 #ifdef AB_CONFIG_TRACE
		if ( ev->DoTrace() )
		{
			this->TraceObject(ev);
			biggerCopy.TraceObject(ev);
		}
#endif /*AB_CONFIG_TRACE*/
 		
  		if ( ev->Good() && biggerCopy.mIntMap_KeyInts ) // successful growth?
  		{
  			// swap vital statistics and storage, to become the biggerCopy
  			
  			ab_map_int* oldKeys = mIntMap_KeyInts;            // save for swap
  			ab_map_int* oldValues = mIntMap_ValueInts;        // save for swap
  			
  			mIntMap_KeyInts = biggerCopy.mIntMap_KeyInts;     // transfer
  			mIntMap_ValueInts = biggerCopy.mIntMap_ValueInts; // transfer
  			
  			biggerCopy.mIntMap_KeyInts = oldKeys;             // to be freed
  			biggerCopy.mIntMap_ValueInts = oldValues;         // to be freed
  			
  			mIntMap_ChangeCount++; // note change (don't copy from biggerCopy)
  			
  			mIntMap_Threshold = biggerCopy.mIntMap_Threshold;
  			mIntMap_Capacity = biggerCopy.mIntMap_Capacity;
  			
#ifdef AB_CONFIG_DEBUG
  			if ( mIntMap_AssocCount != biggerCopy.mIntMap_AssocCount ) // drift?!
  			{
				ev->Break(
					"<ab_IntMap::grow_int_map:drift old=\"%lu\" new=\"%lu\"/>",
					(long) mIntMap_AssocCount,
					(long) biggerCopy.mIntMap_AssocCount);
				this->BreakObject(ev);
			}
#endif /*AB_CONFIG_DEBUG*/
  			
  			mIntMap_AssocCount = biggerCopy.mIntMap_AssocCount;
  		}
  		
  		biggerCopy.CloseIntMap(ev); // deallocate vectors in this map
  	}

	ab_Env_EndMethod(ev)
}

ab_bool ab_IntMap::init_with_size(ab_Env* ev, const ab_KeyMethods* inKeyMethods, 
	ab_num inSizeHint) /*i*/
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "init_with_size")
	
	if ( inKeyMethods )
	{
		mIntMap_Hash = inKeyMethods->mKeyMethods_Hash;
		mIntMap_Equal = inKeyMethods->mKeyMethods_Equal;
		mIntMap_AssocString = inKeyMethods->mKeyMethods_AssocString;
	}

	if ( mIntMap_KeyInts )
	{
		ev->HeapFree(mIntMap_KeyInts);
		mIntMap_KeyInts = 0;
	}
	if ( mIntMap_ValueInts )
	{
		mIntMap_HasValuesToo = AB_kTrue;
		ev->HeapFree(mIntMap_ValueInts);
		mIntMap_ValueInts = 0;
	}
	mIntMap_AssocCount = 0;
	mIntMap_Capacity = 0;
	mIntMap_Threshold = 0;
	if ( ev->Good() ) // proceed to allocate key and value int arrays?
	{
		ab_num capacity = (inSizeHint * 5) / 4; // one quarter more (25%)
		mIntMap_Threshold = inSizeHint;
		mIntMap_Capacity = capacity;
		
		ab_num volume = capacity * sizeof(ab_map_int); // bytes in each array
		mIntMap_KeyInts = (ab_map_int*) ev->HeapAlloc(volume);
		if ( mIntMap_KeyInts ) // allocated keys?
		{
			AB_MEMSET(mIntMap_KeyInts, 0, volume); // fill with all zeroes
 
			if ( mIntMap_HasValuesToo ) // need to allocate values vector?
			{
				mIntMap_ValueInts = (ab_map_int*) ev->HeapAlloc(volume);
				if ( mIntMap_ValueInts ) // allocated values?
					AB_MEMSET(mIntMap_ValueInts, 0, volume); // zero all
			}
		}
	}

	ab_Env_EndMethod(ev)
	return ev->Good();
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // non-poly ab_IntMap methods

ab_IntMap::ab_IntMap(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	const ab_KeyMethods* inKeyMethods, ab_num inSizeHint)
	: ab_Object(inUsage),
	mIntMap_ChangeCount( 0 ),
	mIntMap_Threshold( 0 ),
	mIntMap_Capacity( 0 ),
	mIntMap_AssocCount( 0 ),
	mIntMap_KeyInts( 0 ),
	mIntMap_ValueInts( 0 ),
	
	mIntMap_Hash( 0 ),
	mIntMap_Equal( 0 ),
	mIntMap_AssocString( 0 ),
	
	mIntMap_HasValuesToo( AB_kFalse )

	// defaults to supporting values in addition to keys.  inSizeHint
	// is capped between kMinSizeHin and kMaxSizeHint, and this becomes
	// the new threshold, with capacity equal to 25% more.
{
	if ( inSizeHint < ab_IntMap_kMinSizeHint )
		inSizeHint = ab_IntMap_kMinSizeHint;
	else if ( inSizeHint > ab_IntMap_kMaxSizeHint )
		inSizeHint = ab_IntMap_kMaxSizeHint;
	
	if ( inKeyMethods )
		this->init_with_size(ev, inKeyMethods, inSizeHint);
	else
	{
		ab_KeyMethods defaults(ab_Env_HashKey, ab_Env_EqualKey, ab_Env_AssocString);
		this->init_with_size(ev, &defaults, inSizeHint);
	}
}

ab_IntMap::ab_IntMap(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	const ab_KeyMethods* inKeyMethods, ab_num inSizeHint, ab_bool inValuesToo)
	: ab_Object(inUsage),
	mIntMap_ChangeCount( 0 ),
	mIntMap_Threshold( 0 ),
	mIntMap_Capacity( 0 ),
	mIntMap_AssocCount( 0 ),
	mIntMap_KeyInts( 0 ),
	mIntMap_ValueInts( 0 ),
	
	mIntMap_Hash( 0 ),
	mIntMap_Equal( 0 ),
	mIntMap_AssocString( 0 ),

	mIntMap_HasValuesToo( inValuesToo )

	// map contains no values when inValuesToo is false, which technically
	// creates a simple set and not a map.  inSizeHint
	// is capped between kMinSizeHin and kMaxSizeHint, and this becomes
	// the new threshold, with capacity equal to 25% more.
{
	if ( inSizeHint < ab_IntMap_kMinSizeHint )
		inSizeHint = ab_IntMap_kMinSizeHint;
	else if ( inSizeHint > ab_IntMap_kMaxSizeHint )
		inSizeHint = ab_IntMap_kMaxSizeHint;
			
	if ( inKeyMethods )
		this->init_with_size(ev, inKeyMethods, inSizeHint);
	else
	{
		ab_KeyMethods defaults(ab_Env_HashKey, ab_Env_EqualKey, ab_Env_AssocString);
		this->init_with_size(ev, &defaults, inSizeHint);
	}
}
    	
ab_IntMap::ab_IntMap(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	ab_num inSizeHint, const ab_IntMap& ioOtherMap)
	: ab_Object(inUsage),
	mIntMap_ChangeCount( 0 ),
	mIntMap_Threshold( 0 ),
	mIntMap_Capacity( 0 ),
	mIntMap_AssocCount( 0 ),
	mIntMap_KeyInts( 0 ),
	mIntMap_ValueInts( 0 ),
	
	mIntMap_Hash( ioOtherMap.mIntMap_Hash ),
	mIntMap_Equal( ioOtherMap.mIntMap_Equal ),
	mIntMap_AssocString( ioOtherMap.mIntMap_AssocString ),

	mIntMap_HasValuesToo( ioOtherMap.mIntMap_ValueInts != 0 )

	// The new map contains values provided ioOtherMap does. inSizeHint is
	// capped between kMinSizeHin and kMaxSizeHint, but is then increased
	// to ioOtherMap.GetThreshold() is this is greater, and then used for
	// the new threshold, with capacity equal to 25% more.  The content in
	// ioOtherMap is copied into this one.  (This constructor is used when
	// a map is grown with grow_int_map(), which makes a larger local map
	// before replacing the old slots when this is successful.)
{
	if ( inSizeHint < ab_IntMap_kMinSizeHint )
		inSizeHint = ab_IntMap_kMinSizeHint;
	else if ( inSizeHint > ab_IntMap_kMaxSizeHint )
		inSizeHint = ab_IntMap_kMaxSizeHint;
	
	if ( inSizeHint < ioOtherMap.GetThreshold() )
		inSizeHint = ioOtherMap.GetThreshold();
	
	if ( this->init_with_size(ev, /*inKeyMethods*/ 0, inSizeHint) )
	{
		this->AddTable(ev, ioOtherMap);
	}
}

void ab_IntMap::CloseIntMap(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "CloseIntMap")

	if ( mIntMap_KeyInts )
	{
		ev->HeapFree(mIntMap_KeyInts);
		mIntMap_KeyInts = 0;
	}
	if ( mIntMap_ValueInts )
	{
		ev->HeapFree(mIntMap_ValueInts);
		mIntMap_ValueInts = 0;
	}
	ab_Env_EndMethod(ev)
}

// ````` ````` entire table methods ````` `````  
ab_bool ab_IntMap::AddTable(ab_Env* ev, const ab_IntMap& ioOtherMap) /*i*/
	// add contents of ioOtherMap, and then return ev->Good().
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "AddTable")

	if ( this->IsOpenObject() && ioOtherMap.IsOpenObject() )
	{
		if ( this != &ioOtherMap && ev->Good() )
		{
			ab_map_int* keys = ioOtherMap.mIntMap_KeyInts;
			ab_map_int* end = keys + ioOtherMap.mIntMap_Capacity;
			ab_map_int* cursor = --keys; // prepare for preincrement
			ab_map_int* values = ioOtherMap.mIntMap_ValueInts;

			++mIntMap_ChangeCount; // note change in map (fast_add() does not)
			
			while ( ++cursor < end && ev->Good() )
			{
				ab_map_int k = *cursor;
				if ( k ) // another key in the other table?
				{
					if ( values )
						this->fast_add(ev, k, values[ cursor - keys ]);
					else
						this->fast_add(ev, k, ab_IntMap_kPretendValue);
				}
			}
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return ev->Good();
}

// ````` ````` bucket location ````` `````  

ab_map_int* ab_IntMap::find_bucket(ab_Env* ev, ab_map_int inKey) const /*i*/
{
	ab_map_int* outBucket = 0;
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "find_bucket")
	
	ab_pos pos = this->hash_bucket(ev, inKey);
	ab_map_int* keys = mIntMap_KeyInts;
	ab_map_int* bucket = keys + pos; // first candidate bucket
	
	ab_map_int k = *bucket;
	if ( k && k != inKey ) // first choice already used for some other key?
	{
		ab_map_int* end = keys + mIntMap_Capacity; // one past end, for wrap
		ab_map_int* start = bucket; // where we started, to detect lap
		
		while ( ++bucket != start && ev->Good() ) // have not yet lapped?
		{
			if ( bucket >= end ) // need to wrap?
				bucket = keys; // back to the first bucket in map
				
			k = *bucket;
			if ( !k || this->equal_key(ev, inKey, k) ) // empty? or found key?
			{
				outBucket = bucket;
				break; // use this one, end while loop
			}
		}
	}
	else
		outBucket = bucket;

	ab_Env_EndMethod(ev)
	return outBucket;
}

// ````` ````` adding with less error detection ````` `````  

void ab_IntMap::fast_add(ab_Env* ev, ab_map_int inKey, ab_map_int inValue) /*i*/
{
	if ( mIntMap_AssocCount >= mIntMap_Threshold ) // need bigger map?
		grow_int_map(ev);
		
	if ( ev->Good() ) // no errors?
	{
		ab_map_int* bucket = this->find_bucket(ev, inKey);
		if ( bucket && ( *bucket == inKey || !*bucket ) ) // found key bucket?
		{
			*bucket = inKey;
			ab_map_int* values = mIntMap_ValueInts;
			if ( values ) // also storing values?
				values[ bucket - mIntMap_KeyInts ] = inValue;
				
			++mIntMap_AssocCount; // count additional member
		}
		else ev->NewAbookFault(ab_Map_kFaultNoBucketFound);
	}
}
	
// ````` ````` uid assoc methods ````` `````  

void ab_IntMap::AddAssoc(ab_Env* ev, ab_map_int inKey, ab_map_int inValue) /*i*/
	// add one new association (if an association already exists with 
	// the same key, then the value is simply updated to the new value).
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "AddAssoc")

	if ( this->IsOpenObject() )
	{
		if( inKey ) // non-zero key?
		{
			if ( mIntMap_AssocCount >= mIntMap_Threshold ) // need bigger map?
				grow_int_map(ev);
				
			if ( ev->Good() ) // no errors?
			{
				if ( mIntMap_AssocCount < mIntMap_Threshold ) // enough space?
				{
					ab_map_int* bucket = this->find_bucket(ev, inKey);
					if ( bucket ) // found bucket for this key?
					{
						ab_map_int priorKey = *bucket;
						if ( !priorKey || priorKey == inKey ) // new or replace?
						{
							++mIntMap_ChangeCount; // note map changed
							
							*bucket = inKey;
							ab_map_int* values = mIntMap_ValueInts;
							if ( values ) // also storing values?
								values[ bucket - mIntMap_KeyInts ] = inValue;
								
							++mIntMap_AssocCount;  // count additional member
						}
						else ev->NewAbookFault(ab_Map_kFaultNoBucketFound);
					}
					else ev->NewAbookFault(ab_Map_kFaultNoBucketFound);
				}
				else ev->NewAbookFault(ab_Map_kFaultOverThreashold);
			}
		}
		else ev->NewAbookFault(ab_Map_kFaultZeroKey);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
}
	
ab_map_int ab_IntMap::GetValueForKey(ab_Env* ev, ab_map_int inKey) /*i*/
	// search for one association keyed by inKey, and return the value.
	// Since values cannot be zero, zero is returned to mean "not found".
{
	ab_map_int outValue = 0;
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "GetValueForKey")

	if ( this->IsOpenObject() )
	{
		if ( inKey ) // non-zero key?
		{
			ab_map_int* bucket = this->find_bucket(ev, inKey);
			if ( bucket && *bucket == inKey ) // found a bucket for this key?
			{
				if ( mIntMap_ValueInts ) // values are actually stored?
					outValue = mIntMap_ValueInts[ bucket - mIntMap_KeyInts ];
				else
					outValue = ab_IntMap_kPretendValue; // nominal value
			}
		}
		else
		{
	#ifdef AB_CONFIG_DEBUG
			ev->Break("<ab_IntMap::GetValueForKey:zero:key/>");
	#endif /*AB_CONFIG_DEBUG*/
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return outValue;
}
	
void ab_IntMap::CutAllAssocs(ab_Env* ev) /*i*/
	// remove all associations
{
	ab_Env_BeginMethod(ev, ab_IntMap_kClassName, "CutAllAssocs")

	if ( this->IsOpenObject() )
	{
		mIntMap_AssocCount = 0;
		ab_num volume = mIntMap_Capacity * sizeof(ab_map_int); // total bytes
		if ( volume )
		{
			if ( mIntMap_KeyInts )
				AB_MEMSET(mIntMap_KeyInts, 0, volume); // zero all bytes
			if ( mIntMap_ValueInts )
				AB_MEMSET(mIntMap_ValueInts, 0, volume); // zero all bytes
		}
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
}

/*=============================================================================
 * ab_IntMapIter: iterator for  ab_IntMap
 */

// ````` ````` ````` `````   ````` ````` ````` `````  
// public: // non-poly ab_IntMapIter methods

ab_IntMapIter::ab_IntMapIter(ab_Env* ev, const ab_IntMap* ioIntMap)
	: mIntMapIter_Map( ioIntMap ),
	mIntMapIter_BucketPos( 0xFFFFFFF ),
	mIntMapIter_ChangeCount( ioIntMap->GetChangeCount() - 1 ) // not in sync

    // the ioIntMap object is *not* reference counted, because this iter is
    // expected to be only short-lived inside a single method.
{
	ab_Env_BeginMethod(ev, ab_IntMapIter_kClassName, ab_IntMapIter_kClassName)

	ab_Env_EndMethod(ev)
}
	
ab_map_int ab_IntMapIter::Here(ab_Env* ev, ab_map_int* outValue) /*i*/
{
	ab_map_int outKey = 0;
	ab_Env_BeginMethod(ev, ab_IntMapIter_kClassName, "Here")

	ab_map_int val = 0; // eventually written to *outValue

	const ab_IntMap* map = mIntMapIter_Map;
	if ( map && map->IsOpenObject() ) // okay? open?
	{
		if ( mIntMapIter_ChangeCount == map->GetChangeCount() ) // in sync?
		{
			ab_pos bucket = mIntMapIter_BucketPos;
		  	if ( bucket < map->mIntMap_Capacity ) // iteration still going?
		  	{
				ab_map_int* keys = map->mIntMap_KeyInts; 
				if ( keys ) // have key vector?
				{
					outKey = keys[ bucket ];
					if ( outKey ) // current bucket has key?
					{
						ab_map_int* values = map->mIntMap_ValueInts;
						if ( values )
							val = values[ bucket ]; // actual value
						else
							val = ab_IntMap_kPretendValue; // nominal value
					}
					else ev->NewAbookFault(ab_Map_kFaultIterEmptyBucket);
				}
				else ev->NewAbookFault(ab_Map_kFaultMissingVector);
		  	}
		  	// else return zero to show the iteration has concluded
		}
		else ev->NewAbookFault(ab_ObjectSet_kFaultIterOutOfSync);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	if ( outValue )
		*outValue = val;

	ab_Env_EndMethod(ev)
	return outKey;
}
	
ab_map_int ab_IntMapIter::next_assoc(ab_pos inPos, /*i*/
	ab_map_int* outValue)
{
	ab_map_int outKey = 0;
	
	const ab_IntMap* map = mIntMapIter_Map;
	if ( inPos < map->mIntMap_Capacity ) // iteration is not exhaused?
	{
		ab_map_int* keys = map->mIntMap_KeyInts;
		ab_map_int* end = keys + map->mIntMap_Capacity; // one past end
		
		ab_map_int* cursor = ( keys + inPos ) - 1; // less 1 preps preincrement
		
		while ( ++cursor < end ) // table not yet exhausted?
		{
			outKey = *cursor;
			if ( outKey ) // found next occupied bucket
				break; // end while loop
		}
		inPos = cursor - keys; // update to new bucket (need for BucketPos) 
		
		if ( outKey && outValue ) // found assoc? caller wants value?
		{
			ab_map_int* values = map->mIntMap_KeyInts;
			if ( values ) // values are being stored?
				*outValue = values[ inPos ]; // actual value
			else
				*outValue = ab_IntMap_kPretendValue; // nominal value
		}
	}
	mIntMapIter_BucketPos = inPos; // record where we stopped
	
	return outKey;
}

ab_map_int ab_IntMapIter::First(ab_Env* ev, ab_map_int* outValue) /*i*/
{
	ab_map_int outKey = 0;
	ab_Env_BeginMethod(ev, ab_IntMapIter_kClassName, "First")

	if ( outValue )
		*outValue = 0; // init to default zero

	const ab_IntMap* map = mIntMapIter_Map;
	if ( map && map->IsOpenObject() ) // okay? open?
	{
		// when starting an iteration, put the recorded change count in sync:
		mIntMapIter_ChangeCount = map->GetChangeCount(); // sync
		
		outKey = this->next_assoc(/*inPos*/ 0, outValue);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return outKey;
}
	
ab_map_int ab_IntMapIter::Next(ab_Env* ev, ab_map_int* outValue) /*i*/
{
	ab_map_int outKey = 0;
	ab_Env_BeginMethod(ev, ab_IntMapIter_kClassName, "Next")

	if ( outValue )
		*outValue = 0; // init to default zero

	const ab_IntMap* map = mIntMapIter_Map;
	if ( map && map->IsOpenObject() ) // okay? open?
	{
		if ( mIntMapIter_ChangeCount == map->GetChangeCount() ) // in sync?
		{
			ab_pos bucket = mIntMapIter_BucketPos; // (might be 0xFFFFFFFF)
		  	if ( bucket < map->mIntMap_Capacity ) // iteration not yet ended?
				++bucket; // advance the iteration
				
			outKey = this->next_assoc(/*inPos*/ bucket, outValue);
		}
		else ev->NewAbookFault(ab_ObjectSet_kFaultIterOutOfSync);
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);

	ab_Env_EndMethod(ev)
	return outKey;
}

/* ````` ````` ````` C APIs ````` ````` ````` */
	
/* ````` thumb refcounting ````` */

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_Thumb_Acquire(AB_Thumb* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "Acquire")

	outCount = ((ab_Thumb*) self)->AcquireObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(ab_ref_count) /* abstore.cpp */
AB_Thumb_Release(AB_Thumb* self, AB_Env* cev) /*i*/
{
	ab_ref_count outCount = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "Release")

	outCount = ((ab_Thumb*) self)->ReleaseObject(ev);
	
	ab_Env_EndMethod(ev)
	return outCount;
}

AB_API_IMPL(AB_Thumb*) /* abthumb.cpp */
AB_Store_NewThumb(AB_Store* self, AB_Env* cev, /*i*/
	ab_row_count inRowLimit, ab_num inFileByteCountLimit)
{
	AB_Thumb* outThumb = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, "AB_Store", "NewThumb")

	ab_Thumb* thumb = new(*ev) ab_Thumb(ev, ab_Usage::kHeap);
	if ( thumb )
	{
		if ( ev->Good() )
		{
			outThumb = (AB_Thumb*) thumb;
			thumb->mThumb_FileFormat = ab_File_kLdifFormat;
			if ( !inRowLimit ) 
				inRowLimit = 1;
			
			if ( !inFileByteCountLimit ) 
				inFileByteCountLimit = 256;
				
			thumb->mThumb_RowCountLimit = inRowLimit;
			thumb->mThumb_ByteCountLimit = inFileByteCountLimit;
		}
		else
			thumb->ReleaseObject(ev);
	}
	
	AB_USED_PARAMS_1(ev);

	ab_Env_EndMethod(ev)
	return outThumb;
}

AB_API_IMPL(ab_bool) /* abthumb.cpp */
AB_Thumb_IsProgressFinished(const AB_Thumb* self, AB_Env* cev) /*i*/
{
	ab_bool outBool = AB_kTrue;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "IsProgressFinished")

	ab_Thumb* thumb = (ab_Thumb*) self;
	if ( thumb->IsOpenObject() )
	{
		outBool = thumb->IsProgressFinished();
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outBool;
}

AB_API_IMPL(void) /* abthumb.cpp */
AB_Thumb_SetPortingLimits(AB_Thumb* self, AB_Env* cev, /*i*/
	ab_row_count inRowLimit, ab_num inFileByteCountLimit)
{
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "SetPortingLimits")

	ab_Thumb* thumb = (ab_Thumb*) self;
	if ( thumb->IsOpenObject() )
	{
		if ( !inRowLimit ) 
			inRowLimit = 1;
		
		if ( !inFileByteCountLimit ) 
			inFileByteCountLimit = 256;
			
		thumb->mThumb_RowCountLimit = inRowLimit;
		thumb->mThumb_ByteCountLimit = inFileByteCountLimit;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
}

/*| ImportProgress: return the current position in the import file, and also
**| the actual length of the import file in outFileLength, since this can be
**| used to show percentage progress through the import file.  However, we
**| can only show progress through the current pass through the file, which
**| is returned in outPass (typically either 1 or 2 for first or second pass).
**|
**|| Note that when outPass turns over from 1 to 2, the progress through the
**| file will revert back to a smaller percentage of the file, so a progress
**| bar might want to reveal to users which pass is currently in progress.
|*/
AB_API_IMPL(ab_pos) /* abthumb.cpp */
AB_Thumb_ImportProgress(AB_Thumb* self, AB_Env* cev, /*i*/
	ab_pos* outFileLength, ab_count* outPass)
{
	ab_pos outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "ImportProgress")

	ab_Thumb* thumb = (ab_Thumb*) self;
	if ( thumb->IsOpenObject() )
	{
		if ( !thumb->mThumb_ImportFileLength ) // caller might divide by zero?
			thumb->mThumb_ImportFileLength = 1; // avoid potential zero divisor
		
		outPos = thumb->mThumb_FilePos;
		if ( outFileLength )
			*outFileLength = thumb->mThumb_ImportFileLength;
		if ( outPass )
			*outPass = thumb->mThumb_CurrentPass;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outPos;
}

/*| ExportProgress: return the row position of the row that was last exported,
**| and also the total number of rows to be exported in outTotalRows, since this
**| can be used to show percentage progress through the export task.
|*/
AB_API_IMPL(ab_row_pos) /* abthumb.cpp */
AB_Thumb_ExportProgress(AB_Thumb* self, AB_Env* cev, /*i*/
	ab_row_count* outTotalRows)
{
	ab_row_count outPos = 0;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "ExportProgress")

	ab_Thumb* thumb = (ab_Thumb*) self;
	if ( thumb->IsOpenObject() )
	{
		outPos = thumb->mThumb_RowPos;
		if ( outTotalRows )
			*outTotalRows = thumb->mThumb_ExportTotalRowCount;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outPos;
}

/*| ImportFileFormat: describe the current file format being imported
|*/
AB_API_IMPL(AB_File_eFormat) /* abthumb.cpp */
AB_Thumb_ImportFileFormat(AB_Thumb* self, AB_Env* cev) /*i*/
{
	AB_File_eFormat outFormat = AB_File_kUnknownFormat;
	ab_Env* ev = ab_Env::AsThis(cev);
	ab_Env_BeginMethod(ev, AB_Thumb_kClassName, "ImportFileFormat")

	ab_Thumb* thumb = (ab_Thumb*) self;
	if ( thumb->IsOpenObject() )
	{
		outFormat = (AB_File_eFormat) thumb->mThumb_FileFormat;
	}
	else ev->NewAbookFault(ab_Object_kFaultNotOpen);
	
	ab_Env_EndMethod(ev)
	return outFormat;
}




