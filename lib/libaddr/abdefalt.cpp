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

/* file: abdefalt.cpp
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

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	static const char* ab_Defaults_kClassName /*i*/ = "ab_Defaults";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


#define ab_Defaults_k_column_count /*i*/ 5

static const AB_Column ab_Defaults_g_columns[] = {
	{ // 0
  		/*sColumn_Name*/        (const char*) "givenname",
  		/*sColumn_DisplayName*/ (const char*) "Name",
  		/*sColumn_Uid*/         (ab_column_uid) AB_Column_kGivenName,
  		/*sColumn_SecondSort*/  (ab_column_uid) AB_Column_kFamilyName,
  		/*sColumn_CellSize*/    (ab_cell_size) AB_Column_kSize_GivenName,
  		/*sColumn_CanSort*/     (ab_bool) AB_kTrue
	}
	,{ // 1
  		/*sColumn_Name*/        (const char*) 0,
  		/*sColumn_DisplayName*/ (const char*) "Email",
  		/*sColumn_Uid*/         (ab_column_uid) AB_Column_kEmail,
  		/*sColumn_SecondSort*/  (ab_column_uid) 0,
  		/*sColumn_CellSize*/    (ab_cell_size) AB_Column_kSize_Email,
  		/*sColumn_CanSort*/     (ab_bool) AB_kTrue
	}
	,{ // 2
  		/*sColumn_Name*/        (const char*) "companyname",
  		/*sColumn_DisplayName*/ (const char*) "Organization",
  		/*sColumn_Uid*/         (ab_column_uid) AB_Column_kCompanyName,
  		/*sColumn_SecondSort*/  (ab_column_uid) 0,
  		/*sColumn_CellSize*/    (ab_cell_size) AB_Column_kSize_CompanyName,
  		/*sColumn_CanSort*/     (ab_bool) AB_kTrue
	}
	,{ // 3
  		/*sColumn_Name*/        (const char*) "locality",
  		/*sColumn_DisplayName*/ (const char*) "City",
  		/*sColumn_Uid*/         (ab_column_uid) AB_Column_kLocality,
  		/*sColumn_SecondSort*/  (ab_column_uid) 0,
  		/*sColumn_CellSize*/    (ab_cell_size) AB_Column_kSize_Locality,
  		/*sColumn_CanSort*/     (ab_bool) AB_kTrue
	}
	,{ // 4
  		/*sColumn_Name*/        (const char*) "nickname",
  		/*sColumn_DisplayName*/ (const char*) "Nickname",
  		/*sColumn_Uid*/         (ab_column_uid) AB_Column_kNickname,
  		/*sColumn_SecondSort*/  (ab_column_uid) 0,
  		/*sColumn_CellSize*/    (ab_cell_size) AB_Column_kSize_Nickname,
  		/*sColumn_CanSort*/     (ab_bool) AB_kTrue
	}
	
	,{ // all zeroes for null termination
  		/*sColumn_Name*/        (const char*) 0,
  		/*sColumn_DisplayName*/ (const char*) 0,
  		/*sColumn_Uid*/         (ab_column_uid) 0,
  		/*sColumn_SecondSort*/  (ab_column_uid) 0,
  		/*sColumn_CellSize*/    (ab_cell_size) 0,
  		/*sColumn_CanSort*/     (ab_bool) 0
	}
};

#define ab_Defaults_k_list_cell_count /*i*/ 5

static const AB_Cell ab_Defaults_g_list_cells[] = {
	{ // 0
		/*sCell_Column*/  (ab_column_uid) AB_Column_kFullName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_FullName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 1
		/*sCell_Column*/  (ab_column_uid) AB_Column_kNickname,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Nickname,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 2
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCharSet,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_CharSet,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 3
		/*sCell_Column*/  (ab_column_uid) AB_Column_kDistName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_DistName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 4
		/*sCell_Column*/  (ab_column_uid) AB_Column_kInfo,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Info,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	
	,{ // all zeroes for null termination
		/*sCell_Column*/  (ab_column_uid) 0,
		/*sCell_Size*/    (ab_cell_size) 0,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
};

#define ab_Defaults_k_person_cell_count /*i*/ 26

static const AB_Cell ab_Defaults_g_person_cells[] = {
	{ // 0
		/*sCell_Column*/  (ab_column_uid) AB_Column_kFullName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_FullName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 1
		/*sCell_Column*/  (ab_column_uid) AB_Column_kEmail,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Email,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 2
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCompanyName, // org
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_CompanyName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 3
		/*sCell_Column*/  (ab_column_uid) AB_Column_kLocality, // city
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Locality,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 4
		/*sCell_Column*/  (ab_column_uid) AB_Column_kNickname,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Nickname,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 5
		/*sCell_Column*/  (ab_column_uid) AB_Column_kGivenName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_GivenName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 6
		/*sCell_Column*/  (ab_column_uid) AB_Column_kMiddleName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_MiddleName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 7
		/*sCell_Column*/  (ab_column_uid) AB_Column_kFamilyName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_FamilyName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 8
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCompanyName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_CompanyName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 9
		/*sCell_Column*/  (ab_column_uid) AB_Column_kRegion,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kRegion,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 10
		/*sCell_Column*/  (ab_column_uid) AB_Column_kInfo,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Info,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 11
		/*sCell_Column*/  (ab_column_uid) AB_Column_kHtmlMail,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_HtmlMail,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 12
		/*sCell_Column*/  (ab_column_uid) AB_Column_kTitle,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Title,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 13
		/*sCell_Column*/  (ab_column_uid) AB_Column_kStreetAddress,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_StreetAddress,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 14
		/*sCell_Column*/  (ab_column_uid) AB_Column_kZip,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Zip,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 15
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCountry,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Country,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 16
		/*sCell_Column*/  (ab_column_uid) AB_Column_kWorkPhone,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_WorkPhone,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 17
		/*sCell_Column*/  (ab_column_uid) AB_Column_kHomePhone,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_HomePhone,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 18
		/*sCell_Column*/  (ab_column_uid) AB_Column_kFax,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Fax,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 19
		/*sCell_Column*/  (ab_column_uid) AB_Column_kSecurity,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_Security,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 20
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCoolAddress,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_CoolAddress,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 21
		/*sCell_Column*/  (ab_column_uid) AB_Column_kUseServer,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_UseServer,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 22
		/*sCell_Column*/  (ab_column_uid) AB_Column_kCharSet,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_CharSet,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 23
		/*sCell_Column*/  (ab_column_uid) AB_Column_kPostalAddress,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_PostalAddress,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 24
		/*sCell_Column*/  (ab_column_uid) AB_Column_kIsPerson,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_IsPerson,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	,{ // 25
		/*sCell_Column*/  (ab_column_uid) AB_Column_kDistName,
		/*sCell_Size*/    (ab_cell_size) AB_Column_kSize_DistName,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
	
	,{ // all zeroes for null termination
		/*sCell_Column*/  (ab_column_uid) 0,
		/*sCell_Size*/    (ab_cell_size) 0,
		/*sCell_Length*/  (ab_cell_length) 0,
		/*sCell_Extent*/  (ab_cell_extent) 0,
		/*sCell_Content*/ (char*) 0,
	}
};

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Defaults methods

ab_Row* ab_Defaults::MakeDefaultRow(ab_Env* ev, ab_Table* ioTable) /*i*/
{
	ab_Row* outRow = 0;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "MakeDefaultRow")

	ab_cell_count cellCount = ab_Defaults_k_person_cell_count + 3;
	ab_Row* newRow = new(*ev) ab_Row(ev, ab_Usage::kHeap, ioTable, cellCount);
	if ( newRow )
	{
		if ( ev->Good() && newRow->AddCells(ev, ab_Defaults_g_person_cells) )
			outRow = newRow;
		else
		{
			newRow->CloseObject(ev);
			newRow->ReleaseObject(ev);
		}
	}

	ab_Env_EndMethod(ev)
	return outRow;
}    

ab_Row* ab_Defaults::MakeDefaultCellRow(ab_Env* ev, ab_Table* ioTable, /*i*/
	const AB_Cell* inCells)
{
	ab_Row* outRow = 0;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "MakeDefaultCellRow")

	ab_cell_count cellCount = 0;
	const AB_Cell* c = inCells;
	if ( c ) /* caller supplied a vector of cells? */
	{
		while ( c->sCell_Column ) /* another column to count? */
			++c;

		cellCount = c - inCells; /* number of cells in vector */
		++cellCount; /* one more for future growth */
	}
	ab_Row* newRow = new(*ev) ab_Row(ev, ab_Usage::kHeap, ioTable, cellCount);
	if ( newRow )
	{
		if ( ev->Good() && newRow->AddCells(ev, inCells) )
			outRow = newRow;
		else
		{
			newRow->CloseObject(ev);
			newRow->ReleaseObject(ev);
		}
	}

	ab_Env_EndMethod(ev)
	return outRow;
}    

ab_Row* ab_Defaults::MakeDefaultColumnRow(ab_Env* ev, ab_Table* ioTable, /*i*/
    const AB_Column* inColumns)
{
	ab_Row* outRow = 0;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "MakeDefaultColumnRow")

	ab_cell_count cellCount = 0;
	const AB_Column* c = inColumns;
	if ( c ) /* caller supplied a vector of cells? */
	{
		while ( c->sColumn_Uid ) /* another column to count? */
			++c;

		cellCount = c - inColumns; /* number of cells in vector */
		++cellCount; /* one more for future growth */
	}
	ab_Row* newRow = new(*ev) ab_Row(ev, ab_Usage::kHeap, ioTable, cellCount);
	if ( newRow )
	{
		if ( ev->Good() && newRow->AddColumns(ev, inColumns) )
			outRow = newRow;
		else
		{
			newRow->CloseObject(ev);
			newRow->ReleaseObject(ev);
		}
	}

	ab_Env_EndMethod(ev)
	return outRow;
}    


ab_cell_count ab_Defaults::GetDefaultPersonCells(ab_Env* ev, /*i*/
	AB_Cell* outCells, ab_cell_count inSize, ab_cell_count* outLength)
{
	ab_cell_count outCount = ab_Defaults_k_person_cell_count;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "GetDefaultPersonCells")
	
	ab_cell_count length = 0;
	if ( inSize && outCells ) // any space to receive output?
	{
		ab_cell_count copy = length = inSize;
		if ( length > ab_Defaults_k_person_cell_count )
			length = ab_Defaults_k_person_cell_count; // max person cells
		if ( copy > ab_Defaults_k_person_cell_count + 1 )
			copy = ab_Defaults_k_person_cell_count + 1; // max to copy
			
		memcpy(outCells, ab_Defaults_g_person_cells, copy * sizeof(AB_Cell));
	}
	if ( outLength )
		*outLength = length;

	ab_Env_EndMethod(ev)
	return outCount;
}    
                         
ab_cell_count ab_Defaults::GetDefaultListCells(ab_Env* ev, /*i*/
	AB_Cell* outCells, ab_cell_count inSize, ab_cell_count* outLength)
{
	ab_cell_count outCount = ab_Defaults_k_list_cell_count;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "GetDefaultListCells")
	
	ab_cell_count length = 0;
	if ( inSize && outCells ) // any space to receive output?
	{
		ab_cell_count copy = length = inSize;
		if ( length > ab_Defaults_k_list_cell_count )
			length = ab_Defaults_k_list_cell_count; // max list cells
		if ( copy > ab_Defaults_k_list_cell_count + 1 )
			copy = ab_Defaults_k_list_cell_count + 1; // max to copy
			
		memcpy(outCells, ab_Defaults_g_list_cells, copy * sizeof(AB_Cell));
	}
	if ( outLength )
		*outLength = length;

	ab_Env_EndMethod(ev)
	return outCount;
}    

ab_column_count ab_Defaults::GetDefaultColumns(ab_Env* ev, /*i*/
	AB_Column* outCols, ab_column_count inSize, ab_column_count* outLength)
{
	ab_column_count outCount = ab_Defaults_k_column_count;
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "GetDefaultColumns")
	
	ab_cell_count length = 0;
	if ( inSize && outCols ) // any space to receive output?
	{
		ab_cell_count copy = length = inSize;
		if ( length > ab_Defaults_k_column_count )
			length = ab_Defaults_k_column_count; // max columns
		if ( copy > ab_Defaults_k_column_count + 1 )
			copy = ab_Defaults_k_column_count + 1; // max to copy
			
		memcpy(outCols, ab_Defaults_g_columns, copy * sizeof(AB_Column));
	}
	if ( outLength )
		*outLength = length;

	ab_Env_EndMethod(ev)
	return outCount;
}    

// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Defaults::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
	AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
	sprintf(outXmlBuf,
		"<ab:defaults:str me=\"^%lX\" row=\"#%lX\" st=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
		(long) this,                                // me=\"^%lX\"
		(long) mPart_RowUid,                        // row=\"#%lX\"
		(long) mPart_Store,                         // st=\"^%lX\"
		(unsigned long) mObject_RefCount,           // rc=\"%lu\"
		this->GetObjectAccessAsString(),            // ac=\"%.9s\"
		this->GetObjectUsageAsString()              // us=\"%.9s\"
		);
#else
	*outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
	return outXmlBuf;
}    

void ab_Defaults::CloseObject(ab_Env* ev) /*i*/
{
	if ( this->IsOpenObject() )
	{
		this->MarkClosing();
		this->CloseDefaults(ev);
		this->MarkShut();
	}
}    

void ab_Defaults::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const  /*i*/
{
#ifdef AB_CONFIG_PRINT
	ioPrinter->PutString(ev, "<ab:defaults>");
	char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

	if ( this->IsOpenObject() )
	{
		ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
		ioPrinter->PushDepth(ev); // indent all objects in the list
		
		ioPrinter->NewlineIndent(ev, /*count*/ 1);
		ioPrinter->PutString(ev, mPart_Store->ObjectAsString(ev, xmlBuf));
		
		ioPrinter->PopDepth(ev); // stop indentation
	}
	else // use ab_Object::ObjectAsString() for non-objects:
	{
		ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
	}
	ioPrinter->NewlineIndent(ev, /*count*/ 1);
	ioPrinter->PutString(ev, "</ab:defaults>");
#else /*AB_CONFIG_PRINT*/
	AB_USED_PARAMS_2(ev,ioPrinter);
#endif /*AB_CONFIG_PRINT*/
}    

ab_Defaults::~ab_Defaults()  /*i*/
{
}    

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Defaults methods

ab_Defaults::ab_Defaults(ab_Env* ev, const ab_Usage& inUsage, /*i*/
	 ab_Store* ioStore)
	 : ab_Part(ev, inUsage,
	 	ab_Session::GetGlobalSession()->NewTempRowUid(),
	 	ioStore
	 )
{
}

void ab_Defaults::CloseDefaults(ab_Env* ev) /*i*/
{
	ab_Env_BeginMethod(ev, ab_Defaults_kClassName, "CloseDefaults")

	this->ClosePart(ev);
	
	ab_Env_EndMethod(ev)
}
