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

/* file: abrow.c 
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

#include "vobject.h"

#if !defined(MOZ_NO_LDAP)
    #define NEEDPROTOS
    #include "ldif.h"
#endif

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    static const char* ab_Row_kClassName /*i*/ = "ab_Row";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    static const char* AB_Row_kClassName /*i*/ = "AB_Row";
#endif /* end AB_CONFIG_TRACE_orDEBUG_orPRINT*/


// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Row protected helper methods


/*| grow_cells: make the cell vector bigger if it is not already
**| as great as the input argument newCapacity.  However, if it is already
**| large enough, change nothing and simply return true.  A true return value
**| means the capacity on return is at least as great as newCapacity.
|*/
ab_bool ab_Row::grow_cells(ab_Env* ev, ab_num newCapacity) /*i*/
{
    ab_bool bigEnough = mRow_Capacity >= newCapacity;
    if ( bigEnough ) /* use short circuit early return? */
        return bigEnough;
        
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "grow_cells")
    /*ab_num newCapacity = ((mRow_Capacity * 5) / 4) + 1;*/ /* +25% */
    
    int32 oldSize = sizeof(AB_Cell) * mRow_Count; /* bytes to copy */
    int32 newSize = sizeof(AB_Cell) * newCapacity; /* bytes to alloc */
    AB_Cell* newCells = (AB_Cell*) ev->HeapAlloc(newSize);
    if ( newCells ) /* cell vector allocation succeeded? */
    {
        int32 diffSize = newSize - oldSize;       /* new byte differential */
        ab_u1* newBytes = ((ab_u1*) newCells) + oldSize; /* new space */
        AB_Cell* oldCells = mRow_Cells; /* the cells to deallocate */
        mRow_Cells = newCells;         /* install the new cells */
        mRow_Capacity = newCapacity;   /* now we can hold this many. */
        ++mRow_CellSeed; /* tuple cell structure has changed */

        if ( oldSize ) /* anything to copy? */
        {
            XP_MEMCPY(newCells, oldCells, oldSize); /* copy bytes of old cells */
            XP_MEMSET(oldCells, 0, oldSize); /* clear old for paranoid safety */
        }
        XP_MEMSET(newBytes, 0, diffSize); /* clear new bytes after copied cells */
        
        /* free old cells last from paranoid fear of exceptions: */
        ev->HeapFree(oldCells);            /* deallocate the old cells */
        bigEnough = AB_kTrue;
    }
    // else ev->NewAbookFault(AB_Row_kFaultOutOfMemory);

    ab_Env_EndMethod(ev)
    return bigEnough;
}

/*| zap_cells: finalize and zero all cells, but do not deallocate them.
**| True is returned if and only if the environment shows no errors.
|*/
ab_bool ab_Row::zap_cells(ab_Env* ev) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "zap_cells")
    
    if ( this->IsOpenOrClosingObject() && mRow_Cells )
    {
        AB_Cell* here = mRow_Cells;
        AB_Cell* end = here + mRow_Count; /* one after the last */
        --here;
        
        while ( ++here < end ) /* another cell to finalize? */
            AB_Cell_Finalize(here, ev); /* keep going, error or not */

        mRow_Count = 0; /* no cell is in use now */
        ++mRow_CellSeed; /* cell structure changed */
    }

    ab_Env_EndMethod(ev)
    return ev->Good();
}

/*| sync_cell_columns: if ioTable is not the same as the current table, then
**| check all the cells for nonstandard column uids, which are table dependent,
**| and change these to be right for the new table.  But standard column uids
**| are constant for all address books, so they can stay the same.
**|
**|| The current implementation just raises an error when non standard columns
**| are found. We can do the right thing later, but right now we can get by
**| using only standard columns.
|*/
ab_bool ab_Row::sync_cell_columns(ab_Env* ev, ab_Table* ioTable) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "sync_cell_columns")

    if ( this->IsOpenObject() && ioTable != mRow_Table )
    {
        AB_Cell* here = mRow_Cells;
        AB_Cell* end = here + mRow_Count; /* one after the last */
        --here;
        
        while ( ++here < end && ev->Good() ) /* another cell to check? */
        {
            ab_column_uid col = here->sCell_Column;
            if ( ! AB_Uid_IsStandardColumn(col) )
                ev->NewAbookFault(AB_Row_kFaultNonStandardColumn);
        }
    }

    ab_Env_EndMethod(ev)
    return ev->Good();
}

/*| find_cell: return the cell with column uid equal to inColUid.
|*/
AB_Cell* ab_Row::find_cell(ab_Env* ev, ab_column_uid inColUid) const /*i*/
{
    AB_USED_PARAMS_1(ev);
    if ( this->IsOpenObject() )
    {
        AB_Cell* here = mRow_Cells;
        AB_Cell* end = here + mRow_Count; /* one after the last */
        --here;
        
        while ( ++here < end ) /* another cell to check? */
        {
            if ( here->sCell_Column == inColUid )
                return here;
        }
    }
    return (AB_Cell*) 0;
}

/*| new_cell: grow first if necessary, then alloc a new cell (uninitialized).
|*/
AB_Cell* ab_Row::new_cell(ab_Env* ev) /*i*/
{
    AB_Cell* outCell = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "new_cell")

    ab_cell_count oldCapacity = mRow_Capacity;
    if ( oldCapacity > mRow_Count ) /* true invariant? */
    {
        ab_cell_count remaining = oldCapacity - mRow_Count;
        if ( remaining == 1 ) /* only end cell left? need to grow vector? */
        {
            ab_cell_count newCapacity = ((oldCapacity * 5) / 4) + 1; /* +25% */
            this->grow_cells(ev, newCapacity);
        }
        if ( ev->Good() ) /* no errors? can allocate next free cell? */
        {
            outCell = mRow_Cells + mRow_Count;
            ++mRow_Count;
        }
    }
    else ev->NewAbookFault(AB_Row_kFaultCountNotUnderSize);

    ab_Env_EndMethod(ev)
    return outCell;
}


// ````` ````` ````` `````   ````` ````` ````` `````  
// virtual ab_Object methods

char* ab_Row::ObjectAsString(ab_Env* ev, char* outXmlBuf) const /*i*/
{
    AB_USED_PARAMS_1(ev);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    sprintf(outXmlBuf,
        "<ab:row:str me:tab=\"^%lX:^%lX\" s=\"#%lX\" cap:cnt=\"%lu:%lu\" cs=\"^%lX\" rc=\"%lu\" a=\"%.9s\" u=\"%.9s\"/>",
        (long) this,                             // me:tab=\"^%lX
        (long) mRow_Table,                       // :^%lX\"
        
        (long) mRow_CellSeed,                    // s=\"#%lX\"
        
        (long) mRow_Capacity,                    // cap:cnt=\"%lu
        (long) mRow_Count,                       // :%lu\"
        
        (long) mRow_Cells,                       // cs=\"^%lX\"
        
        (unsigned long) mObject_RefCount,        // rc=\"%lu\"
        this->GetObjectAccessAsString(),         // a=\"%.9s\"
        this->GetObjectUsageAsString()           // u=\"%.9s\"
        );
#else
    *outXmlBuf = 0; /* empty string */
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
    return outXmlBuf;
}

void ab_Row::CloseObject(ab_Env* ev) /*i*/
{
    if ( this->IsOpenObject() )
    {
        this->MarkClosing();
        this->CloseRow(ev);
        this->MarkShut();
    }
}

AB_MODEL_IMPL(void)
AB_Cell_Print(const AB_Cell* self, ab_Env* ev,
    ab_Printer* ioPrinter, const char* inName) /*i*/
{
    ioPrinter->Print(ev, "<ab:cell size=\"%lu\" len=\"%lu\" ex=\"%lu\">",
        self->sCell_Size,     // size=\"%lu\"
        self->sCell_Length,   // len=\"%lu\"
        self->sCell_Extent    // ex=\"%lu\"
        );

    ioPrinter->PushDepth(ev); // indent 

    if ( !inName )
        inName = "";
        
    ioPrinter->NewlineIndent(ev, /*count*/ 1);
    ioPrinter->Print(ev, "<ab:cell:column id=\"#%lX\" name=\"%.128s\"/>",
        (long) self->sCell_Column, inName);

    ioPrinter->NewlineIndent(ev, /*count*/ 1);
    if ( self->sCell_Length > 256 )
    {
        ioPrinter->PutString(ev, "<ab:cell:content>");

        ioPrinter->PushDepth(ev); // indent 
        ioPrinter->NewlineIndent(ev, /*count*/ 1);
        ioPrinter->Hex(ev, self->sCell_Content, self->sCell_Length, /*pos*/ 0);
        ioPrinter->PopDepth(ev); // stop indentation

        ioPrinter->NewlineIndent(ev, /*count*/ 1);
        ioPrinter->PutString(ev, "</ab:cell:content>");
    }
    else
        ioPrinter->Print(ev, "<ab:cell:content val=\"%.128s\"/>",
            self->sCell_Content);
        
    
    ioPrinter->PopDepth(ev); // stop indentation

    ioPrinter->NewlineIndent(ev, /*count*/ 1);
    ioPrinter->PutString(ev, "</ab:cell>");
}


void ab_Row::PrintObject(ab_Env* ev, ab_Printer* ioPrinter) const /*i*/
{
#ifdef AB_CONFIG_PRINT
    ioPrinter->PutString(ev, "<ab:row>");
    char xmlBuf[ ab_Object_kXmlBufSize + 2 ];

    if ( this->IsOpenObject() )
    {
        ioPrinter->PushDepth(ev); // indent all objects in the list

        ioPrinter->NewlineIndent(ev, /*count*/ 1);
        ioPrinter->PutString(ev, this->ObjectAsString(ev, xmlBuf));
        
        ab_Table* table = mRow_Table;
        if ( table )
        {
            ioPrinter->NewlineIndent(ev, /*count*/ 1);
            ioPrinter->PutString(ev, table->ObjectAsString(ev, xmlBuf));
        }
        const AB_Cell* c = mRow_Cells;
        const AB_Cell* end = c + mRow_Count;
        if ( c )
        {
            while ( c < end )
            {
                const char* name = table->GetColumnName(ev, c->sCell_Column);
                ioPrinter->NewlineIndent(ev, /*count*/ 1);
                AB_Cell_Print(c, ev, ioPrinter, name);
            }
        }
        ioPrinter->PopDepth(ev); // stop indentation
    }
    else // use ab_Object::ObjectAsString() for non-objects:
    {
        ioPrinter->PutString(ev, this->ab_Object::ObjectAsString(ev, xmlBuf));
    }
    ioPrinter->NewlineIndent(ev, /*count*/ 1);
    ioPrinter->PutString(ev, "</ab:row>");
#endif /*AB_CONFIG_PRINT*/
}

ab_Row::~ab_Row() /*i*/
{
    AB_ASSERT(mRow_Table==0);
    AB_ASSERT(mRow_Cells==0);
#if AB_CONFIG_TRACE_orDEBUG_orPRINT
    ab_Object* obj = mRow_Table;
    if ( obj )
        obj->ObjectNotReleasedPanic(ab_Row_kClassName);
#endif /*AB_CONFIG_TRACE_orDEBUG_orPRINT*/
}

// ````` ````` ````` `````   ````` ````` ````` `````  
// non-poly ab_Row methods

ab_Row::ab_Row(ab_Env* ev, const ab_Usage& inUsage, /*i*/
        ab_Table* ioTable, ab_cell_count inHint)
    : ab_Object(inUsage),
    mRow_Table(ioTable)
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "ab_Row")

    if ( inHint < ab_Row_kMinCellHint )      /* too small? */
        inHint = ab_Row_kMinCellHint;        /* make larger */
    else if ( inHint > ab_Row_kMaxCellHint ) /* too large? */
        inHint = ab_Row_kMaxCellHint;        /* make smaller */

    if ( ioTable )
    {
        if ( !ioTable->AcquireObject(ev) )
            mRow_Table = 0; // drop the reference if acquire fails
    }
    else ev->NewAbookFault(AB_Row_kFaultNullTable);

    // start with zeroes until the cells are successfully allocated:
    mRow_Count = mRow_Capacity = 0;
    mRow_Cells = 0;
    
    // random seed values are slightly preferable:
    mRow_CellSeed ^= (ab_num) this ^ (ab_num) ioTable ^ (ab_num) &inHint;

    if ( ev->Good() )
    {
        int32 size = sizeof(AB_Cell) * inHint; /* bytes to alloc */
        mRow_Cells = (AB_Cell*) ev->HeapAlloc(size);
        if ( mRow_Cells ) /* cell vector allocation succeeded? */
        {
            XP_MEMSET(mRow_Cells, 0, size); /* clear all cells */
            mRow_Count = 0;           /* No cells used now, but ... */
            mRow_Capacity = inHint;   /* ... we can hold this many. */
        }
        // else ev->NewAbookFault(AB_Row_kFaultOutOfMemory);
    }
    ab_Env_EndMethod(ev)
}

void ab_Row::CloseRow(ab_Env* ev) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "CloseRow")
    
    ab_Object* obj = mRow_Table;
    if ( obj )
    {
        mRow_Table = 0;
        obj->ReleaseObject(ev);
    }
    if ( mRow_Cells )
    {
        this->zap_cells(ev);
        ev->HeapFree(mRow_Cells);
        mRow_Cells = 0;
        ++mRow_CellSeed;
    }

    ab_Env_EndMethod(ev)
}

ab_bool ab_Row::ChangeTable(ab_Env* ev, ab_Table* ioTable) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "ChangeTable")
    
    if ( this->IsOpenObject() )
    {
        if ( ioTable )
        {
            if ( this->sync_cell_columns(ev, ioTable) )
            {
                if ( ioTable->AcquireObject(ev) )
                {
                    ab_Table* table = mRow_Table;
                    if ( table )
                    {
                        mRow_Table = 0;
                        table->ReleaseObject(ev);
                    }
                    mRow_Table = ioTable;
                }
            }
        }
        else ev->NewAbookFault(AB_Row_kFaultNullTable);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

ab_row_uid ab_Row::FindRowUid(ab_Env* ev) const /*i*/
    // is this row a duplicate of a row already inside the row's table?
    // (according to some (unspecified here) method of identifying rows
    // uniquely -- probably fullname plus email).
{
    ab_row_uid outUid = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "FindRowUid")

    if ( ev->Good() )
    {
        ab_RowContent* content = this->get_row_content(ev);
        if ( content )
        {   
            outUid = content->FindRow(ev, this);
        }
    }

    ab_Env_EndMethod(ev)
    return outUid;
}

ab_Row* ab_Row::DuplicateRow(ab_Env* ev) const /*i*/
{
    ab_Row* outRow = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "DuplicateRow")

    if ( this->IsOpenObject() )
    {
        ab_cell_count count = this->CountCells();
        ab_Row* newRow = new(*ev)
            ab_Row(ev, ab_Usage::kHeap, mRow_Table, count);
        if ( newRow )
        {
            if ( ev->Good() && newRow->AddCells(ev, mRow_Cells) )
                newRow->CopyRow(ev, this);

            if ( ev->Good() )
                outRow = newRow;
            else
            {
                newRow->CloseObject(ev);
                newRow->ReleaseObject(ev);
            }
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outRow;
}

static const char* ab_Row_g_column_ldif_types[ ] = {

    // zero can be used to completely suppress a particular ldif attribute:

    "person",           // AB_Attrib_kIsPerson = 0,  /* 00 - "isperson" (t or f) */ 
    "modtime",          // AB_Attrib_kChangeSeed,    /* 01 - "changeseed" */
    "cn",               // AB_Attrib_kFullName,      /* 02 - "fullname" */
    "xmozillanickname", // AB_Attrib_kNickname,      /* 03 - "nickname" */
    (const char*) 0,    // AB_Attrib_kMiddleName,    /* 04 - "middlename" */
    
    "sn",               // AB_Attrib_kFamilyName,    /* 05 - "familyname" */
    "o",                // AB_Attrib_kCompanyName,   /* 06 - "companyname" */
    "st",               // AB_Attrib_kRegion,        /* 07 - "region" */
    "mail",             // AB_Attrib_kEmail,         /* 08 - "email" */
    "description",      // AB_Attrib_kInfo,          /* 09 - "info" */  
    
    "xmozillausehtmlmail", // AB_Attrib_kHtmlMail,   /* 10 - "htmlmail" (t or f) */      
    "expandedname",     // AB_Attrib_kExpandedName,  /* 11 - "expandedname" */
    "title",            // AB_Attrib_kTitle,         /* 12 - "title" */
    "streetaddress",    // AB_Attrib_kAddress,       /* 13 - "address" */
    "postalcode",       // AB_Attrib_kZip,           /* 14 - "zip" */
    
    "countryname",      // AB_Attrib_kCountry,       /* 15 - "country" */
    "telephonenumber",  // AB_Attrib_kWorkPhone,     /* 16 - "workphone" */
    "homephone",        // AB_Attrib_kHomePhone,     /* 17 - "homephone" */
    (const char*) 0,    // AB_Attrib_kSecurity,      /* 18 - "security" */
    "xmozillaconference",      // AB_Attrib_kCoolAddress,   /* 19 - "cooladdress" (conf related) */ 
    
    "xmozillauseconferenceserver", // AB_Attrib_kUseServer,   /* 20 - "useserver" (conf related) */ 
    "pagerphone",       // AB_Attrib_kPager,         /* 21 - "pager" */
    "facsimiletelephonenumber", // AB_Attrib_kFax,   /* 22 - "fax" */
    "cn",               // AB_Attrib_kDisplayName,   /* 23 - "displayname" */
    "sender",           // AB_Attrib_kSender,        /* 24 - "sender" (mail and news) */
    
    "subject",          // AB_Attrib_kSubject,       /* 25 - "subject" */
    "body",             // AB_Attrib_kBody,          /* 26 - "body" */
    "date",             // AB_Attrib_kDate,          /* 27 - "date" */
    "priority",         // AB_Attrib_kPriority,      /* 28 - "priority" (mail only) */
    "msgstatus",        // AB_Attrib_kMsgStatus,     /* 29 - "msgstatus" */
    
    "to",               // AB_Attrib_kTo,            /* 30 - "to" */
    "cc",               // AB_Attrib_kCC,            /* 31 - "cc" */
    "toorcc",           // AB_Attrib_kToOrCC,        /* 32 - "toorcc" */
    "cn",               // AB_Attrib_kCommonName,    /* 33 - "commonname" (LDAP only) */
    "mail",             // AB_Attrib_k822Address,    /* 34 - "822address" */
    
    "phonenumber",      // AB_Attrib_kPhoneNumber,   /* 35 - "phonenumber" */
    "o",                // AB_Attrib_kOrganization,  /* 36 - "organization" */
    "ou",               // AB_Attrib_kOrgUnit,       /* 37 - "orgunit" */
    "locality",         // AB_Attrib_kLocality,      /* 38 - "locality" */
    "streetaddress",    // AB_Attrib_kStreetAddress, /* 39 - "streetaddress" */
    
    "size",             // AB_Attrib_kSize,          /* 40 - "size" */
    "anytext",          // AB_Attrib_kAnyText,       /* 41 - "anytext" (any header or body) */
    "keywords",         // AB_Attrib_kKeywords,      /* 42 - "keywords" */
    "dn",               // AB_Attrib_kDistName,      /* 43 - "distname" (distinguished name) */ 
    "objectclass",      // AB_Attrib_kObjectClass,   /* 44 - "objectclass" */
         
    "jpegfile",         // AB_Attrib_kJpegFile,      /* 45 - "jpegfile" */
    "location",         // AB_Attrib_kLocation,      /* 46 - "location" (result list only */
    "messagekey",       // AB_Attrib_kMessageKey,    /* 47 - "messagekey" (message result elems) */
    "ageindays",        // AB_Attrib_kAgeInDays,     /* 48 - "ageindays" (purging old news) */
    "givenname",        // AB_Attrib_kGivenName,     /* 49 - "givenname" (sorting LDAP results) */

    "sn",               // AB_Attrib_kSurname,       /* 50 - "surname" */
    "folderinfo",       // AB_Attrib_kFolderInfo,    /* 51 - "folderinfo" (view thread context) */
    "custom1",          // AB_Attrib_kCustom1,       /* 52 - "custom1" (custom LDAP attribs) */
    "custom2",          // AB_Attrib_kCustom2,       /* 53 - "custom2" */
    "custom3",          // AB_Attrib_kCustom3,       /* 54 - "custom3" */
    
    "custom4",          // AB_Attrib_kCustom4,       /* 55 - "custom4" */
    "custom5",          // AB_Attrib_kCustom5,       /* 56 - "custom5" */
    "messageid",        // AB_Attrib_kMessageId,     /* 57 - "messageid" */
    "homeurl",          // AB_Attrib_kHomeUrl,       /* 58 - "homeurl" */
    "workurl",          // AB_Attrib_kWorkUrl,       /* 59 - "workurl" */
    
    "imapurl",          // AB_Attrib_kImapUrl,       /* 60 - "imapurl" */
    "notifyurl",        // AB_Attrib_kNotifyUrl,     /* 61 - "notifyurl" */
    "prefurl",          // AB_Attrib_kPrefUrl,       /* 62 - "prefurl" */
    "pageremail",       // AB_Attrib_kPagerEmail,    /* 63 - "pageremail" */
    "parentphone",      // AB_Attrib_kParentPhone,   /* 64 - "parentphone" */
    
    "gender",           // AB_Attrib_kGender,        /* 65 - "gender" */
    "postOfficeBox",    // AB_Attrib_kPostalAddress, /* 66 - "postaladdress" */
    "employeeid",       // AB_Attrib_kEmployeeId,    /* 67 - "employeeid" */
    "agent",            // AB_Attrib_kAgent,         /* 68 - "agent" */
    "bbs",              // AB_Attrib_kBbs,           /* 69 - "bbs" */
    
    "bday",             // AB_Attrib_kBday,          /* 70 - "bday" (birthdate) */
    "calendar",         // AB_Attrib_kCalendar,      /* 71 - "calendar" */
    "car",              // AB_Attrib_kCar,           /* 72 - "car" */
    "carphone",         // AB_Attrib_kCarPhone,      /* 73 - "carphone" */
    "categories",       // AB_Attrib_kCategories,    /* 74 - "categories" */
    
    "cell",             // AB_Attrib_kCell,          /* 75 - "cell" */
    "cellphone",        // AB_Attrib_kCellPhone,     /* 76 - "cellphone" */
    (const char*) 0,    // AB_Attrib_kCharSet,       /* 77 - "charset" (cs, csid) */
    "class",            // AB_Attrib_kClass,         /* 78 - "class" */
    "geo",              // AB_Attrib_kGeo,           /* 79 - "geo" */
    
    "gif",              // AB_Attrib_kGif,           /* 80 - "gif" */
    "key",              // AB_Attrib_kKey,           /* 81 - "key" (publickey) */
    "language",         // AB_Attrib_kLanguage,      /* 82 - "language" */
    "logo",             // AB_Attrib_kLogo,          /* 83 - "logo" */
    "modem",            // AB_Attrib_kModem,         /* 84 - "modem" */
    
    "msgphone",         // AB_Attrib_kMsgPhone,      /* 85 - "msgphone" */
    "n",                // AB_Attrib_kN,             /* 86 - "n" */
    "note",             // AB_Attrib_kNote,          /* 87 - "note" */
    "pagerphone",       // AB_Attrib_kPagerPhone,    /* 88 - "pagerphone" */
    "pgp",              // AB_Attrib_kPgp,           /* 89 - "pgp" */
    
    "photo",            // AB_Attrib_kPhoto,         /* 90 - "photo" */
    "rev",              // AB_Attrib_kRev,           /* 91 - "rev" */
    "role",             // AB_Attrib_kRole,          /* 92 - "role" */
    "sound",            // AB_Attrib_kSound,         /* 93 - "sound" */
    "sortstring",       // AB_Attrib_kSortString,    /* 94 - "sortstring" */
    
    "tiff",             // AB_Attrib_kTiff,          /* 95 - "tiff" */
    "tz",               // AB_Attrib_kTz,            /* 96 - "tz" (timezone) */
    "uid",              // AB_Attrib_kUid,           /* 97 - "uid" (uniqueid) */
    "version",          // AB_Attrib_kVersion,       /* 98 - "version" */
    "voice",            // AB_Attrib_kVoice,         /* 99 - "voice" */
};

const char* ab_Row::get_col_as_ldif_type(ab_Env* ev, /*i*/
    ab_column_uid inColUid) const
{
    const char* outType = 0;

    if ( AB_Uid_IsStandardColumn(inColUid) )
    {
        ab_num attrib = AB_ColumnUid_AsDbUid(inColUid);
        if ( attrib <= AB_Attrib_kVoice )
            outType = ab_Row_g_column_ldif_types[ attrib ];
    }
    
    AB_USED_PARAMS_1(ev);
    return outType;
}

void ab_Row::write_cell_as_ldif_attrib(ab_Env* ev, const AB_Cell* inCell, /*i*/
    ab_Stream* ioStream) const
{
    ab_cell_length length = inCell->sCell_Length;
    if ( length )
    {
        // cast away const on value & type because the ldif routines rudely insist:
        char* value = (char*) inCell->sCell_Content;
        char* type = (char*) this->get_col_as_ldif_type(ev, inCell->sCell_Column);
        if ( value && *value && type )
        {
            if ( inCell->sCell_Column == AB_Column_kHtmlMail )
            {
                if (*value == 't' || *value == 'T')
                {
                    value = "TRUE";
                    length = 4;
                }
                else
                {
                    value = "FALSE";
                    length = 5;
                }
            }
            char* line = ldif_type_and_value(type, value, length);
            if ( line )
            {
                ioStream->PutString(ev, line);
            }
            XP_FREEIF(line);
        }
    }
}

void ab_Row::write_person_class(ab_Env* ev, ab_Stream* ioStream) const /*i*/
{
    char* line = ldif_type_and_value("objectclass", "top", /*length*/ 3);
    if ( line )
    {
        ioStream->PutString(ev, line);
        XP_FREEIF(line);
    }
    if ( ev->Good() )
    {
        line = ldif_type_and_value("objectclass", "person", /*length*/ 6);
        if ( line )
        {
            ioStream->PutString(ev, line);
            XP_FREEIF(line);
        }
    }
}

void ab_Row::write_list_class(ab_Env* ev, ab_Stream* ioStream) const /*i*/
{
    char* line = ldif_type_and_value("objectclass", "top", /*length*/ 3);
    if ( line )
    {
        ioStream->PutString(ev, line);
        XP_FREEIF(line);
    }
    if ( ev->Good() )
    {
        char* value = "groupOfNames";
        line = ldif_type_and_value("objectclass", value, AB_STRLEN(value));
        if ( line )
        {
            ioStream->PutString(ev, line);
            XP_FREEIF(line);
        }
    }
}

void ab_Row::write_leading_dn_to_ldif_stream(ab_Env* ev, /*i*/
    ab_Stream* ioStream) const
{
    if ( ev->Good() )
    {
        char dnBuf[ 256 + 2 ];
        if ( this->GetDistinguishedName(ev, dnBuf) )
        {
            ioStream->PutString(ev, "dn: ");
            ioStream->PutStringThenNewline(ev, dnBuf);
        }
    }
}

void ab_Row::write_member_dn_to_ldif_stream(ab_Env* ev, /*i*/
    ab_Stream* ioStream) const
{
    if ( ev->Good() )
    {
        char dnBuf[ 256 + 2 ];
        if ( this->GetDistinguishedName(ev, dnBuf) )
        {
            ioStream->PutString(ev, "member: ");
            ioStream->PutStringThenNewline(ev, dnBuf);
        }
    }
}

void ab_Row::add_cells_for_ldif_dn_attrib(ab_Env* ev) /*i*/
{
    if ( ev->Good() )
        this->AddCell(ev, AB_Column_kFullName, /*cellSize*/ 256);
    if ( ev->Good() )
        this->AddCell(ev, AB_Column_kEmail, /*cellSize*/ 256);
}

void ab_Row::write_list_members(ab_Env* ev, ab_row_uid inRowUid, /*i*/
    ab_Stream* ioStream) const
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "write_list_members")
    
    AB_Table* rowTable = mRow_Table->AsSelf();
    AB_Env* cev = ev->AsSelf();
    AB_Table* list = AB_Table_AcquireRowChildrenTable(rowTable, cev, inRowUid);
    if ( list ) // able to acquire the table representing the mailing list?
    {
        ab_row_count rowCount = AB_Table_CountRows(list, cev);
        if ( rowCount && ev->Good() ) // nonzero number of members? no errors?
        {
            ab_row_count size = rowCount + 3; // add slop for good measure
            ab_num volume = size * sizeof(ab_row_uid); // bytes in the vector
            ab_row_uid* v = (ab_row_uid*) ev->HeapAlloc(volume);
            if ( v ) // successfully allocated vector to receive sequence of ids?
            {
                ab_row_pos first = 1; // one based index
                ab_row_count actual = AB_Table_GetRows(list, cev, v, size, first);
                if ( actual && ev->Good() ) // read any members? no errors?
                {
                    ab_Table* listTable = ab_Table::AsThis(list);
                    ab_Row row(ev, ab_Usage::kStack, listTable, /*hint*/ 32);
                    row.add_cells_for_ldif_dn_attrib(ev);
                    AB_Row* crow = row.AsSelf();
                    ab_row_uid* cursor = v;
                    ab_row_uid* end = v + actual; // one past last member
                    --cursor; // prepare for preincrement
                    
                    while ( ++cursor < end && ev->Good() ) // another member?
                    {
                        if ( row.ClearCells(ev) ) // emptied old content?
                        {
                            if ( AB_Row_ReadTableRow(crow, cev, *cursor) ) // got?
                            {
                                row.write_member_dn_to_ldif_stream(ev, ioStream);
                            }
                        }
                    }
                    row.CloseObject(ev);
                }
                ev->HeapFree(v); // always free the vector when done
            }
        }
        AB_Table_Release(list, cev);
    }
    ab_Env_EndMethod(ev)
}

ab_bool ab_Row::WriteToLdifStream(ab_Env* ev, ab_row_uid inRowUid, /*i*/
    ab_Stream* ioStream) const
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "WriteToLdifStream")

    if ( this->IsOpenObject() )
    {
        // write dn attribute first (required by ldif internet draft?)
        this->write_leading_dn_to_ldif_stream(ev, ioStream);
            
        if ( ev->Good() ) // no problem writing the dn attribute?
        {
            ab_bool isPerson = AB_kTrue;
            AB_Cell* here = mRow_Cells;
            AB_Cell* end = here + mRow_Count; /* one after the last */
            --here;
            
            if ( mRow_Count )
            {
                while ( ++here < end && ev->Good() ) /* another cell to write? */
                {
                    if ( here->sCell_Column == AB_Column_kIsPerson ) // person?
                    {
                        unsigned byte = *here->sCell_Content;
                        if ( byte != 't' && byte != 'T' )
                            isPerson = AB_kFalse;
                    }
                    else if ( here->sCell_Column != AB_Column_kDistName )
                    {
                        // write all other cell values as ldif attributes:
                        this->write_cell_as_ldif_attrib(ev, here, ioStream);
                    }
                }
                if ( ev->Good() )
                {
                    if ( isPerson ) // need to write object class for person?
                    {
                        this->write_person_class(ev, ioStream);
                    }
                    else // write list object class, then write the members also
                    {
                        this->write_list_class(ev, ioStream);
                        if ( ev->Good() )
                            this->write_list_members(ev, inRowUid, ioStream);
                    }
                }
                if ( ev->Good() ) // separate ldif records with a blank line
                    ioStream->WriteNewlines(ev, /*count*/ 1);
            }
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);
    
    ab_Env_EndMethod(ev)
    return ev->Good();
}

ab_bool ab_Row::GetDistinguishedName(ab_Env* ev, /*i*/
    char* outBuf256) const
    // Write the distinguished name attribute to outBuf256, which must be
    // at least 256 bytes in size.  The DN is currently composed of
    // FullName (aka FullName,CommonName, cn, or DisplayName) using the
    // AB_Column_kFullName column, and the email address using the
    // AB_Column_kEmail column.  If either of these cells is not in the
    // row, then an empty string is used instead.  The DN is formated as
    // follows "cn=<full name>,mail=<email>". True when ev->Good() is true.
    // for example "cn=John Smith,mail=johns@netscape.com"
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetDistinguishedName")

    if ( outBuf256 )
    {
        *outBuf256 = 0;
        
        AB_Cell* cell = this->GetColumnCell(ev, AB_Column_kDistName);
        if ( cell ) // has an explicit DN already?
        {
            XP_SPRINTF(outBuf256, "%.250s", cell->sCell_Content);
        }
        else if ( ev->Good() ) // construct a composite dn?
        {
            const char* email = 0;
            const char* fullName = this->GetColumnValue(ev, AB_Column_kFullName);
            if ( ev->Good() )
                email = this->GetColumnValue(ev, AB_Column_kEmail);
                
            if ( ev->Good() )
            {
                XP_SPRINTF(outBuf256, "cn=%.110s,mail=%.110s",
                    fullName, email);
            }
        }
    }
    ab_Env_EndMethod(ev)
    return ev->Good();
}

char* ab_Row::AsVCardString(ab_Env* ev) const /*i*/
    // returned string allocated by ab_Env::CopyString() which advertises
    // itself as using XP_ALLOC for space allocation.  A null pointer is
    // typically returned in case of error.  (It would not be that safe
    // to return an empty static string which could not be deallocated.)
{
    char* outString = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "AsVCardString")
    
    static const char* empty = "";

    if ( this->IsOpenObject() )
    {
        ab_bool isList = this->IsListRow(ev); // perhaps list nature is needed
        VObject* v = newVObject(VCCardProp);
        if ( v && ev->Good() )
        {
            // note GetColumnValue() never returns null -- only empty string
            const char* full = 0;
            const char* family = 0;
            const char* given = 0;
            const char* company = 0;
            const char* locality = 0;
            const char* region = 0;
            const char* address = 0;
            const char* zip = 0;
            const char* poaddr = 0;
            const char* country = 0;
            const char* email = 0;
            const char* title = 0;
            const char* work = 0;
            const char* fax = 0;
            const char* home = 0;
            const char* info = 0;
            const char* cool = 0;
            const char* use = 0;
            const char* html = 0;
            
            VObject* t = 0;
            
            full = this->GetColumnValue(ev, AB_Column_kFullName);
            if ( ev->Good() )
            {
                if ( *full ) // nonempty full name?
                    addPropValue(v, VCFullNameProp, full);
                    
                t = addProp(v, VCNameProp);

                family = this->GetColumnValue(ev, AB_Column_kFamilyName);
                if ( ev->Good() )
                {
                    if ( *family )
                        addPropValue(t, VCFamilyNameProp, family);
                        
                    given = this->GetColumnValue(ev, AB_Column_kGivenName);
                    if ( ev->Good() )
                    {
                        if ( *given )
                            addPropValue(t, VCGivenNameProp, given);
                            
                        company = this->GetColumnValue(ev, AB_Column_kCompanyName);
                        if ( ev->Good() && *company )
                        {
                            t = addProp(v, VCOrgProp);
                            addPropValue(t, VCOrgNameProp, company);
                        }
                    }
                }
            }
            
            if ( ev->Good() )
            {
                locality = this->GetColumnValue(ev, AB_Column_kLocality);

                if ( ev->Good() )
                    region = this->GetColumnValue(ev, AB_Column_kRegion);

                if ( ev->Good() )
                    address = this->GetColumnValue(ev, AB_Column_kAddress);
                    
                if ( ev->Good() )
                    zip = this->GetColumnValue(ev, AB_Column_kZip);
                    
                if ( ev->Good() )
                    poaddr = this->GetColumnValue(ev, AB_Column_kPostalAddress);
                    
                if ( ev->Good() )
                    country = this->GetColumnValue(ev, AB_Column_kCountry);
            }
            if ( ev->Good() && *locality || *region || *address || *zip ||
                 *poaddr || *country )
            {
                t = addProp(v, VCAdrProp);
                if ( !*country )
                    addProp(t, VCDomesticProp);
                addPropValue(t, VCPostalBoxProp, poaddr);
                addPropValue(t, VCExtAddressProp, "");
                addPropValue(t, VCStreetAddressProp, address);
                addPropValue(t, VCCityProp, locality);
                addPropValue(t, VCRegionProp, region);
                addPropValue(t, VCPostalCodeProp, zip);
                addPropValue(t, VCCountryNameProp, country);
            }
                
            if ( ev->Good() )
            {
                email = this->GetColumnValue(ev, AB_Column_kEmail);
                if ( ev->Good() )
                {
                    if ( *email )
                    {
                        t = addPropValue(v, VCEmailAddressProp, email);
                        addProp(t, VCInternetProp);
                    }
                    
                    title = this->GetColumnValue(ev, AB_Column_kTitle);
                    if ( ev->Good() )
                    {
                        if ( *title )
                            addPropValue(v, VCTitleProp, title);
                            
                        work = this->GetColumnValue(ev, AB_Column_kWorkPhone);
                        if ( ev->Good() )
                        {
                            if ( *work )
                            {
                                t = addPropValue(v, VCTelephoneProp, work);
                                addProp(t, VCWorkProp);
                            }
                        
                            fax = this->GetColumnValue(ev, AB_Column_kFax);
                            if ( ev->Good() && *fax )
                            {
                                t = addPropValue(v, VCTelephoneProp, fax);
                                addProp(t, VCFaxProp);
                            }
                        }
                    }
                }
            }
                
            if ( ev->Good() )
            {
                home = this->GetColumnValue(ev, AB_Column_kHomePhone);
                if ( ev->Good() )
                {
                    if ( *home )
                    {
                        t = addPropValue(v, VCTelephoneProp, home);
                        addProp(t, VCHomeProp);
                    }
                    
                    info = this->GetColumnValue(ev, AB_Column_kInfo);
                    if ( ev->Good() )
                    {
                        if ( *info )
                            addPropValue(v, VCNoteProp, info);
                            
                        cool = this->GetColumnValue(ev, AB_Column_kCoolAddress);
                        if ( ev->Good() )
                        {
                            if ( *cool )
                            {
                                t = addProp(v, VCCooltalk);
                                addPropValue(t, VCCooltalkAddress, cool);
                            }

                            use = this->GetColumnValue(ev, AB_Column_kUseServer);
                            if ( ev->Good() && *use )
                            {
                                t = addProp(v, VCCooltalk);
                                addPropValue(t, VCUseServer, use);
                            }
                        }
                    }
                }
            }
                
            if ( ev->Good() )
            {
                html = this->GetColumnValue(ev, AB_Column_kHtmlMail);
                if ( ev->Good() )
                {
                    if ( *html && *html == 't' )
                        addPropValue(v, VCUseHTML, "TRUE");
                    else
                        addPropValue(v, VCUseHTML, "FALSE");
                }
            }

            if ( ev->Good() )
            {
                addPropValue(v, VCVersionProp, "2.1");

                int length = 0;
                char* vCard = writeMemVObjects(0, &length, v);
                outString = ev->CopyString(vCard);
            }
            
            cleanVObject(v);
        }
        else if ( ev->Good() )
            ev->NewAbookFault(AB_Env_kFaultOutOfMemory);
        
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outString;
}

ab_bool ab_Row::CopyRow(ab_Env* ev, const ab_Row* other) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "CopyRow")

    if ( this->IsOpenObject() )
    {
        AB_Cell* here = mRow_Cells;
        AB_Cell* end = here + mRow_Count; /* one after the last */
        --here;

#ifdef AB_CONFIG_DEBUG
        ab_Store* store = other->mRow_Table->GetPartStore();
        if ( store != mRow_Table->GetPartStore() )
            ev->Break("store does not match");
#endif /*AB_CONFIG_DEBUG*/  
    
        while ( ++here < end && ev->Good() )
        {
            const AB_Cell* c = other->find_cell(ev, here->sCell_Column);
            if ( c ) /* does other have a corresponding column? */
            {
                AB_Cell_Copy(here, ev, c); /* duplicate content from inRow */
            }
            else /* make our cell empty to reflect no content in inRow */
            {
                here->sCell_Length = 0; /* no content */
                here->sCell_Content[ 0 ] = '\0'; /* and null terminated */
            }
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}
    
ab_bool ab_Row::ClearCells(ab_Env* ev) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "ClearCells")

    if ( this->IsOpenObject() )
    {
        AB_Cell* here = mRow_Cells;
        AB_Cell* end = here + mRow_Count; /* one after the last */
        --here;
        
        while ( ++here < end ) /* another cell to clear? */
        {
            here->sCell_Length = 0;
            here->sCell_Content[ 0 ] = '\0';
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

ab_bool ab_Row::WriteCell(ab_Env* ev, const char* inContent, /*i*/
    ab_column_uid inCol)
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "WriteCell")
    
    if ( !inContent ) // caller unexpectedly passed a null pointer?
        inContent = ""; // treat it the same as empty string.

    if ( this->IsOpenObject() )
    {
        ab_cell_length length = AB_STRLEN(inContent);
        ab_cell_size size = length + 1;
        AB_Cell cell;
        cell.sCell_Column = inCol;
        cell.sCell_Size = size;
        cell.sCell_Length = length;
        cell.sCell_Extent = 0;
        cell.sCell_Content = (char*) inContent; /* cast away const */
        this->PutCell(ev, &cell);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

#define AB_Row_k_min_good_cell_size /*i*/ 12 /* avoid tiny cell sizes */

ab_bool ab_Row::PutCell(ab_Env* ev, const AB_Cell* inCell) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "PutCell")
    
    ab_cell_length length = inCell->sCell_Length;
    ab_cell_size size = length + 1; /* need at least one byte more */
    if ( size < AB_Row_k_min_good_cell_size ) /* small? */
        size = AB_Row_k_min_good_cell_size; /* a bit bigger */

    if ( this->IsOpenObject() )
    {
        AB_Cell* c = this->AddCell(ev, inCell->sCell_Column, size);
        if ( c ) /* cell found and/or added to tuple? */
        {
            if ( length ) /* anything to copy? */
                XP_MEMCPY(c->sCell_Content, inCell->sCell_Content, length);
            c->sCell_Content[ length ] = '\0';
            c->sCell_Length = length;
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}


ab_bool ab_Row::PutHexLongCol(ab_Env* ev, long inLong, /*i*/
    ab_column_uid inColUid, ab_bool inDoAdd)
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "PutHexLongCol")
    
    if ( this->IsOpenObject() )
    {
        AB_Cell* cell = this->find_cell(ev, inColUid);
        ab_bool needNew = ( !cell && inDoAdd );
        ab_bool needMore = ( cell && cell->sCell_Size < 32 );
        if ( ( needNew || needMore) && ev->Good() )
            cell = this->AddCell(ev, inColUid, 32);
            
        if ( cell )
            AB_Cell_WriteHexLong(cell, ev, inLong);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}


ab_bool ab_Row::PutShortCol(ab_Env* ev, short inShort, /*i*/
    ab_column_uid inColUid, ab_bool inDoAdd)
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "PutShortCol")
    
    if ( this->IsOpenObject() )
    {
        AB_Cell* cell = this->find_cell(ev, inColUid);
        ab_bool needNew = ( !cell && inDoAdd );
        ab_bool needMore = ( cell && cell->sCell_Size < 16 );
        if ( ( needNew || needMore) && ev->Good() )
            cell = this->AddCell(ev, inColUid, 16);
            
        if ( cell )
            AB_Cell_WriteShort(cell, ev, inShort);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

ab_bool ab_Row::PutBoolCol(ab_Env* ev, short inBool, /*i*/
    ab_column_uid inColUid, ab_bool inDoAdd)
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "PutBoolCol")
    
    if ( this->IsOpenObject() )
    {
        AB_Cell* cell = this->find_cell(ev, inColUid);
        ab_bool needNew = ( !cell && inDoAdd );
        ab_bool needMore = ( cell && cell->sCell_Size < 4 );
        if ( ( needNew || needMore) && ev->Good() )
            cell = this->AddCell(ev, inColUid, 4);
            
        if ( cell )
            AB_Cell_WriteBool(cell, ev, inBool);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}


ab_cell_count ab_Row::GetCells(ab_Env* ev, AB_Cell* outVector, /*i*/
    ab_cell_count inSize, ab_cell_count* outLength) const
{
    ab_cell_count outCount = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetCells")

    if ( this->IsOpenObject() )
    {
        ab_cell_count length = mRow_Count; /* all cells in tuple */
        outCount = length; /* all the cells in the tuple */
        
        if ( length > inSize ) /* not enough space for them all? */
            length = inSize;   /* return only this many */
            
        if ( outVector ) /* does caller want cell data returned? */
        {
            if ( inSize > length ) /* room for end null cell? */
                XP_MEMSET(outVector + length, 0, sizeof(AB_Cell)); /* clear end */
                
            if ( length ) /* any cells to copy? */
                XP_MEMCPY(outVector, mRow_Cells, length * sizeof(AB_Cell));
        }
        if ( outLength ) /* caller wants to know number of cells written? */
            *outLength = length;
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outCount;
}

AB_Cell* ab_Row::GetCellAt(ab_Env* ev, ab_cell_pos inCellPos) const /*i*/
{
    AB_Cell* outCell = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetCellAt")

    if ( this->IsOpenObject() )
    {
        if ( inCellPos && inCellPos <= mRow_Count ) /* in range? */
            outCell = mRow_Cells + (inCellPos - 1);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outCell;
}

AB_Cell* ab_Row::GetColumnCell(ab_Env* ev, /*i*/
     ab_column_uid inColUid) const
{
    AB_Cell* outCell = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetColumnCell")

    if ( this->IsOpenObject() )
    {
        outCell = this->find_cell(ev, inColUid);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outCell;
}


ab_bool ab_Row::GetCellNames(ab_Env* ev, const char** inGivenName,
    const char** inFamilyName, const char** inFullName) const
    // For each pointer that is non-null, perform the equivalent
    // of calling GetCellContent() for appropriate columns (which are
    // AB_Column_kGivenName AB_Column_kFamilyName AB_Column_kFullName)
    // and return the cell content pointer in the location pointed at
    // by each argument.  The boolean return equals ev->Good().
{
    ab_bool found = AB_kFalse;
    if ( this->IsOpenObject() )
    {
        if ( inGivenName && ev->Good() )
        {
            AB_Cell* cell = this->find_cell(ev, AB_Column_kGivenName);
            if ( cell && *cell->sCell_Content )
            {
                *inGivenName = cell->sCell_Content;
                found = AB_kTrue;
            }
            else
                *inGivenName = 0;
        }
        if ( inFamilyName && ev->Good() )
        {
            AB_Cell* cell = this->find_cell(ev, AB_Column_kFamilyName);
            if ( cell && *cell->sCell_Content )
            {
                *inFamilyName = cell->sCell_Content;
                found = AB_kTrue;
            }
            else
                *inFamilyName = 0;
        }
        if ( inFullName && ev->Good() )
        {
            AB_Cell* cell = this->find_cell(ev, AB_Column_kFullName);
            if ( cell && *cell->sCell_Content )
            {
                *inFullName = cell->sCell_Content;
                found = AB_kTrue;
            }
            else
                *inFullName = 0;
        }
    }
    else
    {
        ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetCellContent")
        
        ev->NewAbookFault(ab_Object_kFaultNotOpen);
        
        ab_Env_EndMethod(ev)
    }
    return (found && ev->Good());
}


const char* ab_Row::GetColumnValue(ab_Env* ev, /*i*/
    ab_column_uid inColUid) const
        // returns empty static string when no such cell exists
{
    const char* outContent = ""; // default to static empty string
    if ( this->IsOpenObject() )
    {
        AB_Cell* cell = this->find_cell(ev, inColUid);
        if ( cell && *cell->sCell_Content )
            outContent = cell->sCell_Content;
    }
    else
    {
        ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetColumnValue")
        
        ev->NewAbookFault(ab_Object_kFaultNotOpen);
        
        ab_Env_EndMethod(ev)
    }
    return outContent;
}

const char* ab_Row::GetCellContent(ab_Env* ev, /*i*/
    ab_column_uid inColUid) const
        // returns null when no such cell exists
{
    const char* outContent = 0;
    if ( this->IsOpenObject() )
    {
        AB_Cell* cell = this->find_cell(ev, inColUid);
        if ( cell && *cell->sCell_Content )
            outContent = cell->sCell_Content;
    }
    else
    {
        ab_Env_BeginMethod(ev, ab_Row_kClassName, "GetCellContent")
        
        ev->NewAbookFault(ab_Object_kFaultNotOpen);
        
        ab_Env_EndMethod(ev)
    }
    return outContent;
}

ab_bool ab_Row::AddColumns(ab_Env* ev, const AB_Column* inCollVector) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "AddColumns")

    if ( this->IsOpenObject() )
    {
        const AB_Column* c = inCollVector;
        if ( c ) // caller passed nonzero pointer?
        {
            while ( c->sColumn_Uid && ev->Good() ) /* another col to add? */
            {
                this->AddCell(ev, c->sColumn_Uid, c->sColumn_CellSize);
                ++c; /* advance for next loop test */
            }
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

ab_bool ab_Row::AddCells(ab_Env* ev, const AB_Cell* inCellVector) /*i*/
{
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "AddCells")

    if ( this->IsOpenObject() )
    {
        const AB_Cell* c = inCellVector;
        if ( c ) // caller passed nonzero pointer?
        {
            while ( c->sCell_Column && ev->Good() ) /* another col to add? */
            {
                this->AddCell(ev, c->sCell_Column, c->sCell_Size);
                ++c; /* advance for next loop test */
            }
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ev->Good();
}

AB_Cell* ab_Row::AddCell(ab_Env* ev, ab_column_uid inColUid, /*i*/
    ab_num inCellSize)
{
    AB_Cell* outCell = 0;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "AddCell")

    if ( this->IsOpenObject() )
    {
        if ( AB_Uid_IsColumn(inColUid) ) /* column is valid? */
        {
            if ( inCellSize ) /* size is valid positive value? */
            {
                AB_Cell* c = this->find_cell(ev, inColUid);
                if ( c ) /* had such a cell already? */
                {
                    if ( inCellSize > c->sCell_Size ) /* need bigger cell? */
                        AB_Cell_Grow(c, ev, inCellSize);
                }
                else /* need to allocate a new cell */
                {
                    c = this->new_cell(ev);
                    if ( c ) /* allocated a cell in the tuple */
                    {
                        const char* content = 0; /* no starting content */
                        AB_Cell_Init(c, ev, inColUid, inCellSize, content);
                    }
                }
                if ( c && ev->Good() )
                    outCell = c;
            }
            else ev->NewAbookFault(AB_Cell_kFaultZeroCellSize);
        }
        else ev->NewAbookFault(AB_Cell_kFaultBadColumnUid);
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return outCell;
}

ab_bool ab_Row::CutCell(ab_Env* ev, ab_column_uid inColUid) /*i*/
{
    ab_bool foundCell = AB_kFalse;
    ab_Env_BeginMethod(ev, ab_Row_kClassName, "CutCell")

    if ( this->IsOpenObject() )
    {
        AB_Cell* c = this->find_cell(ev, inColUid);
        if ( c ) /* found the cell for column inColUid? */
        {
            ab_cell_count count = mRow_Count; /* old cell count */
            
            foundCell = AB_kTrue;
            ++mRow_CellSeed; /* changing the cell structure: */
            AB_Cell_Finalize(c, ev); /* destroy and fill with all zeroes */
            
            if ( count > 1 ) /* are there other cells? */
            {
                /* If cut cell was not the last, we need to fill the hole. */
                ab_cell_pos index = c - mRow_Cells; /* found index */
                if ( --count > index ) /* more after cut cell? */
                {
                    AB_Cell* last = mRow_Cells + count; /* last cell */
                    XP_MEMCPY(c, last, sizeof(AB_Cell)); /* move last to hole */
                    XP_MEMSET(last, 0, sizeof(AB_Cell)); /* clear empty spot */
                }
            }
            else
                count = 0; /* no more cells */
                
            mRow_Count = count;
        }
    }
    else ev->NewAbookFault(ab_Object_kFaultNotOpen);

    ab_Env_EndMethod(ev)
    return ( foundCell && ev->Good() );
}


// ````` ````` ````` `````   ````` ````` ````` `````  
// C row methods

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */

AB_FILE_IMPL(ab_RowSet*)
AB_Row_get_row_set(const AB_Row* self, ab_Env* ev)
{
    ab_RowSet* rowSet = 0;
    ab_error_uid newError = 0;

    ab_Table* table = ((const ab_Row*) self)->GetRowTable();
    if ( table->IsOpenObject() )
    {
        rowSet = table->TableRowSet();
        if ( rowSet )
        {
            if ( !rowSet->IsOpenObject() )
            {
                newError = ab_Object_kFaultNotOpen;
                rowSet = 0;
            }
        }
        else newError = AB_Table_kFaultNullRowSetSlot;
    }
    else newError = AB_Table_kFaultNotOpenTable;
    
    if ( newError )
    {
        ab_Env_BeginMethod(ev, AB_Row_kClassName, "get_row_set")
        ev->NewAbookFault(newError);
        if ( table )
            table->CloseObject(ev);
        ab_Env_EndMethod(ev)
    }
    
    return rowSet;
}

ab_RowContent* ab_Row::get_row_content(ab_Env* ev) const /*i*/
{
    ab_RowContent* content = 0;
    ab_error_uid newError = 0;

    ab_Table* table = this->GetRowTable();
    if ( table->IsOpenObject() )
    {
        content = table->TableRowContent();
        if ( content )
        {
            if ( !content->IsOpenObject() )
            {
                newError = ab_Object_kFaultNotOpen;
                content = 0;
            }
        }
        else newError = AB_Table_kFaultNullRowContentSlot;
    }
    else newError = AB_Table_kFaultNotOpenTable;
    
    if ( newError )
    {
        ab_Env_BeginMethod(ev, AB_Row_kClassName, "get_row_content")
        ev->NewAbookFault(newError);
        if ( table )
            table->CloseObject(ev);
        ab_Env_EndMethod(ev)
    }
    
    return content;
}



AB_API_IMPL(AB_Row*)
AB_Row_MakeRowClone(const AB_Row* self, AB_Env* cev) /*i*/
    /*- MakeRowClone allocates a duplicate of row self with exactly the same
    table, cell structure, and cell content. A caller might use this method to
    create a copy of a row prior to making changes when the original row
    must be kept intact. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return (AB_Row*) ((const ab_Row*) self)->DuplicateRow(ev);
}

AB_API_IMPL(ab_ref_count)
AB_Row_Acquire(AB_Row* self, AB_Env* cev) /*i*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->AcquireObject(ev);
}

AB_API_IMPL(ab_ref_count)
AB_Row_Release(AB_Row* self, AB_Env* cev) /*i*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->ReleaseObject(ev);
}

AB_API_IMPL(ab_bool)
AB_Row_CopyRowContent(AB_Row* self, AB_Env* cev, const AB_Row* inOtherRow) /*i*/
    /*- CopyRowContent makes all cells in self contain the same content as
    corresponding cells (with the same sCell_Column) in other. This affects
    cell content only, and does not change the cell structure of self at all,
    so number and size of cells does not change. If a cell in self is too
    small to receive all content in other, the content is truncated rather 
    than enlarging the cell (in contrast to AB_Row_BecomeRow()). -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ((ab_Row*) self)->CopyRow(ev, (const ab_Row*) inOtherRow);
    return ev->Good();
}

AB_API_IMPL(ab_bool)
AB_Row_BecomeRowClone(AB_Row* self, AB_Env* cev, const AB_Row* inOtherRow) /*i*/
    /*- BecomeRowClone causes self to be a clone of other, with exactly the
    same cell structure and cell content. (If self and other belong to
    different tables, then self changes its table to that of other.) -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_Env_BeginMethod(ev, AB_Row_kClassName, "BecomeRowClone")

    AB_USED_PARAMS_2(self,inOtherRow);

    ev->NewAbookFault(AB_Env_kFaultMethodStubOnly);
    
    ab_Env_EndMethod(ev)
    return 0;
}

AB_API_IMPL(AB_Table*)
AB_Row_GetTable(const AB_Row* self, AB_Env* cev) /*i*/
    /*- GetTable returns the table that created this row.
    (Null returns on error.)-*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_Table* table = ((const ab_Row*) self)->GetRowTable();
    if ( !table->IsOpenObject() )
    {
        ev->NewAbookFault(AB_Table_kFaultNotOpenTable);
        table = 0;
    }

    return table->AsSelf();
}

AB_API_IMPL(ab_bool)
AB_Row_ChangeTable(AB_Row* self, AB_Env* cev, AB_Table* ioTable) /*i*/
    /*- ChangeTable sets the table for this row to table (and this does any
    necessary table reference counting). -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->ChangeTable(ev, ab_Table::AsThis(ioTable));
}

AB_API_IMPL(ab_row_uid)
AB_Row_NewTableRowAt(const AB_Row* self, AB_Env* cev, ab_row_pos inPos) /*i*/
    /*- New creates a new row in the table at position pos by writing the 
    cells specified by this row. Zero is returned on error (zero is never a 
    valid uid), and the specific error is indicated by ab_Env_GetError(). 

    The position pos can be any value, not just one to
    AB_Table_CountRows(). If zero, this means put the new row wherever
    seems best. If greater than the number of rows, it means append to the
    end. If the table is currently sorted in some order, pos is ignored and
    the new row is goes in sorted position. -*/
{
    ab_row_uid outUid = 0;
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
    if ( content )
    {   
        outUid = content->NewRow(ev, (ab_Row*) self, inPos);
        if ( outUid )
        {
            ab_RowSet* rowSet = AB_Row_get_row_set(self, ev);
            if ( rowSet )
            {
                rowSet->AddRow(ev, outUid, inPos);
            }
        }
    }
    if ( ev->Bad() )
        outUid = 0;
        
    return outUid;
}


AB_API_IMPL(ab_bool)
AB_Row_ReadTableRowAt(AB_Row* self, AB_Env* cev, ab_row_pos inPos) /*i*/
    /*- ReadTableRowAt reads the row in the table at postion pos, filling cell
    values from the persistent content for the row found in the table. All cells
    are given values read from row id in the table, and this includes making
    cells empty when the persistent row has no such cell value. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_bool outBool = AB_kFalse;
    ab_RowSet* rowSet = AB_Row_get_row_set(self, ev);
    if ( rowSet )
    {
        ab_row_uid rowUid = rowSet->GetRow(ev, inPos);
        if ( rowUid )
        {
            ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
            if ( content )
                outBool = content->GetCells(ev, (ab_Row*) self, rowUid);
        }
    }
    return ( outBool && ev->Good() );
}

AB_API_IMPL(ab_bool)
AB_Row_ReadTableRow(AB_Row* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
    /*- ReadTableRow reads the row in the table known by id, filling cell
    values from the persistent content for the row found in the table. All cells
    are given values read from row id in the table, and this includes making
    cells empty when the persistent row has no such cell value. If cells in
    self are too small to read the entire content from the table, only the -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
    if ( content )
        return content->GetCells(ev, (ab_Row*) self, inRowUid);
    else
        return AB_kFalse;
}

AB_API_IMPL(ab_bool)
AB_Row_GrowToReadEntireTableRow(AB_Row* self, AB_Env* cev, /*i*/
         ab_row_uid inRowUid, ab_cell_size inMaxCellSize)
    /*- GrowToReadEntireTableRow reads the row in the table known by id,
    filling cell values from the persistent content for the row found in the
    table. All cells are given values read from row id in the table, and this
    includes making cells empty when the persistent row has no such cell
    value. 

    In contrast to AB_Row_ReadTableRow() which does not change cell
    structure to read content from the table, GrowToReadEntireTableRow
    will add cells as needed and grow the size of cells as needed in order to
    hold every byte of every persistent cell value. In other words,
    GrowToReadEntireTableRow will read the entire persistent content of
    the table row while growing to the extent necessary to capture all this
    content. 

    The maxCellSize parameter can be zero to cause it to be ignored, but if
    nonzero, maxCellSize is used to cap cell growth so to no more than
    maxCellSize. This lets callers attempt to accomodate all persistent
    content up to some reasonable threshold over which the caller might no
    longer care whether more cell content is present. So if a persistent value
    is too big to fit in a cell, and is also bigger than maxCellSize, then the
    cell has its size increased only to maxCellSize before reading the cell
    value. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
    if ( content )
        return content->GetAllCells(ev, (ab_Row*) self,
            inRowUid, inMaxCellSize);
    else
        return AB_kFalse;
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(ab_row_pos)
AB_Row_UpdateTableRow(const AB_Row* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
    /*- UpdateTableRow writes part of the row known by id in the table,
    updating all cells with non-empty content in self, so that empty cells in
    self have no effect on the persistent row in the table. Also, cells not
    described at all in self are not touched at all in the table. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
    if ( content )
    {
        ab_RowSet* rowSet = AB_Row_get_row_set(self, ev);
        if ( rowSet )
        {
            if ( content->PutCells(ev, (const ab_Row*) self, inRowUid) )
                return rowSet->FindRow(ev, inRowUid);
        }
    }
    return 0;
}

AB_API_IMPL(ab_row_pos)
AB_Row_ResetTableRow(const AB_Row* self, AB_Env* cev, ab_row_uid inRowUid) /*i*/
    /*- ResetTableRow writes all of the row known by id in the table, updating
    all of the table row cells, whether they are described by self or not.
    Cells not in self and empty cells in self are removed from row id.
    Non-empty cells in self have their values written to row id so that each
    table row cell contains only the content described by self. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    ab_RowContent* content = ((ab_Row*) self)->get_row_content(ev);
    if ( content )
    {
        ab_RowSet* rowSet = AB_Row_get_row_set(self, ev);
        if ( rowSet )
        {
            if ( content->PutAllCells(ev, (const ab_Row*) self, inRowUid) )
                return rowSet->FindRow(ev, inRowUid);
        }
    }
    return 0;
}

/* ----- ----- ----- ----- Cells ----- ----- ----- ----- */


AB_API_IMPL(ab_bool)
AB_Row_ClearAllCells(AB_Row* self, AB_Env* cev) /*i*/
    /*- ClearAllCells makes all the cells in this row empty of content. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->ClearCells(ev);
}

AB_API_IMPL(ab_bool)
AB_Row_WriteCell(AB_Row* self, AB_Env* cev,  /*i*/
    const char* inCellContent, ab_column_uid inColumnUid)
    /*- WriteCell ensures that row self has a cell with column col and sets
    the content of this cell to content, with length strlen(content). If
    the row did not previously have such a cell, it gets one just as if
    AB_Row_AddCell() had been called. Also, if the length of content is
    greater than the old size of the cell, the cell's size is increased to make it
    big enough to hold all the content (also just as if AB_Row_AddCell() had
    been called). 

    (WriteCell is implemented by calling AB_Row_PutCell().) -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->WriteCell(ev, inCellContent, inColumnUid);
}

AB_API_IMPL(ab_bool)
AB_Row_PutCell(AB_Row* self, AB_Env* cev, const AB_Cell* inCell) /*i*/
    /*- PutCell is the internal form of WriteCell which assumes the length of
    the content to write is already known. Only the sCell_Content,
    sCell_Length, and sCell_Column slots are used from c. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->PutCell(ev, inCell);
}

AB_API_IMPL(ab_cell_count)
AB_Row_CountCells(const AB_Row* self, AB_Env* cev) /*i*/
    /*- CountCells returns the number of cells in this row. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    if ( ((const ab_Row*) self)->IsOpenObject() )
        return ((const ab_Row*) self)->CountCells();
    else
    {
        ev->NewAbookFault(ab_Object_kFaultNotOpen);
        return 0;
    }
}

AB_API_IMPL(ab_cell_count)
AB_Row_GetCells(const AB_Row* self, AB_Env* cev,
    AB_Cell* outVector, ab_cell_count inSize, ab_cell_count* outLength) /*i*/
    /*- GetCells returns a copy of the row's cells in outVector. The actual
    number of row cells is returned as the function value, but the size of
    outVector is assumed to be inSize, so no more than inSize cells will
    be written to outVector. The actual number of cells written is returned
    in outLength (unless outLength is a null pointer to suppress this
    output). 

    If inSize is greater than the number of cells, N, then outVector[N] will
    be null terminated by placing all zero values in all the cell slots. 

    Each sCell_Content slot points to space owned by the row. This space
    might change whenever the row changes, so callers must not cause the
    row to change while cell content is still being accessed. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((const ab_Row*) self)->GetCells(ev, outVector, inSize, outLength);
}

/* ----- ----- ----- ----- ----- ----- ----- ----- ----- ----- */


AB_API_IMPL(const AB_Cell*)
AB_Row_GetCellAt(const AB_Row* self, AB_Env* cev, ab_cell_pos inPos) /*i*/
    /*- GetCellAt returns a pointer to a AB_Cell inside the self instance. This
    is a potentially dangerous exposure of internal structure for the sake of
    performance. Callers are expected to use this cell only long enough to
    see cell slots like sCell_Content that might be needed, and then forget
    this pointer as soon as possible. 

    Whenever the row is changed so that cell structure is modified, this
    AB_Cell instance might no longer exist afterwards. So using this cell
    must be only of transient nature. Clients should call GetCellAt again
    later, each time a cell is desired. 

    GetCellAt returns a null pointer if pos is zero, or if pos is greater
    than the number of cells (AB_Row_CountCells()). The cells have
    one-based positions numbered from one to the number of cells. 

    The cell returned must not be modified, on pain of undefined behavior.
    Callers must not assume anything about adjacency of other cells in the
    row. Cells might be stored discontiguously. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((const ab_Row*) self)->GetCellAt(ev, inPos);
}

/*3456789_123456789_123456789_123456789_123456789_123456789_123456789_12345678*/

AB_API_IMPL(const AB_Cell*)
AB_Row_GetColumnCell(const AB_Row* self, AB_Env* cev, /*i*/
    ab_column_uid inColumnUid)
    /*- GetColumnCell is just like AB_Row_GetCellAt() except that the cell is
    found by matching the column uid. If the row has no such column, then
    a null pointer is returned. -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((const ab_Row*) self)->GetColumnCell(ev, inColumnUid);
}

AB_API_IMPL(ab_bool)
AB_Row_AddCells(AB_Row* self, AB_Env* cev, const AB_Cell* inVector) /*i*/
    /*- AddCells changes the row's cell structure to include the cells described
    in inVector which must be null terminated by a zero value in the
    sCell_Column slot after the last cell to be added. Only the
    sCell_Column and sCell_Size slots matter and other slots are ignored.

    The sCell_Column slot values should be all distinct, without duplicate
    columns. If inVector has duplicate columns, the last one wins and no
    error occurs (so callers should check before calling if they care). 

    AddCells is implemented by calling AB_Row_AddCell(). -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->AddCells(ev, inVector);
}

AB_API_IMPL(AB_Cell*)
AB_Row_AddCell(AB_Row* self, AB_Env* cev, ab_column_uid inColumnUid, /*i*/
    ab_cell_size inCellSize)
    /*- AddCell gives the row another cell for column id which can hold size
    substract one content bytes. Both id and size must be nonzero, and
    id must denote a valid column in the address book, and size must be no
    greater than AB_Cell_kMaxCellSize. If the cell known by id
    previously existed, then it's size is changed to equal the maximum of
    either the old size or the new size. (To shrink the size of a cell, remove
    it first with AB_Row_CutCell() and then re-add the cell. The specified
    size behavior here is most convenient for upgrading rows to hold bigger
    content.) -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->AddCell(ev, inColumnUid, inCellSize);
}

AB_API_IMPL(ab_bool)
AB_Row_CutCell(AB_Row* self, AB_Env* cev, ab_column_uid inColumnUid) /*i*/
    /*- CutCell removes any cell identified by column uid id. True is returned
    only if such a cell existed (and was removed). False returns when no
    such cell was found (or if an error occurs). No error occurs from cutting
    a cell which does not exist, because the desired end result -*/
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->CutCell(ev, inColumnUid);
}

AB_API_IMPL(char *) /* abrow.c */
AB_Row_AsVCardString(AB_Row* self, AB_Env* cev)
{
    ab_Env* ev = ab_Env::AsThis(cev);
    return ((ab_Row*) self)->AsVCardString(ev);
}


