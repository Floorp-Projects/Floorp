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

#include "xp.h"
#include "abcom.h"
#include "xp_mem.h"
#include "xpassert.h"
#include "xp_list.h"
#include "xp_mcom.h"
#include "vobject.h"
#include "vcc.h"

/***************************************************************************
    Export functions used with the new address book.....

 **************************************************************************/
const char * getVObjectIDForAttribID(AB_AttribID attrib); // adding prototype 
AB_AttributeValue * getValueForAttribute(AB_AttribID attrib, AB_AttributeValue * values, uint16 numItems);

AB_API int AB_ExportVCardToTempFile (const char * vCard, char** filename)
{
    int status = AB_SUCCESS;

    XP_File fp;

#if !defined(MOZADDRSTANDALONE)
    if (vCard && filename)
    {
        *filename = WH_TempName(xpFileToPost, "nsmail");
        if ((*filename))
        {
            fp = XP_FileOpen(*filename, xpFileToPost, XP_FILE_WRITE_BIN);
            if (fp)
            {
                XP_FileWrite(vCard, XP_STRLEN(vCard), fp);
                XP_FileClose(fp);
            }
            else 
                status = AB_FAILURE; // eUnableToOpenTMPFile

        }
    }
#else
    XP_ASSERT (FALSE);
#endif

    return status;
}

const char * getVObjectIDForAttribID(AB_AttribID attrib)
{
    switch (attrib)
    {
    case AB_attribEmailAddress:
        return VCEmailAddressProp;
    case AB_attribLocality:
        return VCCityProp;
    case AB_attribFaxPhone:
    case AB_attribWorkPhone:
    case AB_attribHomePhone:
        return VCTelephoneProp;
    case AB_attribFamilyName:
        return VCFamilyNameProp;
    case AB_attribDisplayName:
        return VCFullNameProp;
    case AB_attribGivenName:
        return VCGivenNameProp;
    case AB_attribCompanyName:
        return VCOrgNameProp;
    case AB_attribZipCode: 
        return VCPostalCodeProp;
    case AB_attribRegion: 
        return VCRegionProp;
    case AB_attribStreetAddress:
        return VCStreetAddressProp;
    case AB_attribPOAddress:
        return VCPostalBoxProp;
    case AB_attribCountry:
        return VCCountryNameProp;
    case AB_attribTitle:
        return VCTitleProp;
    case AB_attribCoolAddress:
        return VCCooltalkAddress;
    case AB_attribUseServer: 
        return VCUseServer;
    case AB_attribHTMLMail:
        return VCUseHTML;
    case AB_attribInfo:
        return VCNoteProp;
    case AB_attribCellularPhone:
        return VCCellularProp;
    case AB_attribWinCSID:
        return VCCIDProp;

    default:
        return NULL;
    }
}

AB_AttributeValue * getValueForAttribute(AB_AttribID attrib, AB_AttributeValue * values, uint16 numItems)
{
    if (values)
        for (uint16 i = 0; i < numItems; i++)
            if (values[i].attrib == attrib)
                return &values[i];
    return NULL;
}

int AB_ConvertAttribValuesToVCard(AB_AttributeValue * values, uint16 numItems, char ** result)
{
    VObject* vObj = newVObject(VCCardProp);
    VObject* t;
    AB_AttributeValue * value = NULL;

    /* Big flame coming....so Vobject is not designed at all to work with  an array of 
    attribute values. It wants you to have all of the attributes easily available. You
    cannot add one attribute at a time as you find them to the vobject. Why? Because
    it creates a property for a particular type like phone number and then that property
    has multiple values. This implementation is not pretty. I can hear my algos prof
    yelling from here.....I have to do a linear search through my attributes array for
    EACH vcard property we want to set. *sigh* One day I will have time to come back
    to this function and remedy this O(m*n) function where n = # attribute values and
    m = # of vcard properties....

  */

    if (values)
    {
        value = getValueForAttribute(AB_attribDisplayName, values, numItems);
        if (value && value->u.string)
            addPropValue(vObj,VCFullNameProp, value->u.string);
        t = addProp(vObj, VCNameProp);
        value = getValueForAttribute(AB_attribFamilyName, values, numItems);
        if (value && value->u.string)
            addPropValue(t, VCFamilyNameProp, value->u.string);
        value = getValueForAttribute(AB_attribGivenName, values, numItems);
        if (value && value->u.string)
            addPropValue(t, VCGivenNameProp, value->u.string);

        value = getValueForAttribute(AB_attribCompanyName, values, numItems);
        if (value && value->u.string)
        {
            t = addProp(vObj, VCOrgProp);
            addPropValue(t, VCOrgNameProp, value->u.string);
        }
        value = getValueForAttribute(AB_attribPOAddress, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCPostalBoxProp, value->u.string);

        }
        value = getValueForAttribute(AB_attribStreetAddress, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCStreetAddressProp, value->u.string);
        }
        value = getValueForAttribute(AB_attribLocality, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCCityProp, value->u.string);
        }
        value = getValueForAttribute(AB_attribRegion, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCRegionProp, value->u.string);
        }
        value = getValueForAttribute(AB_attribZipCode, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCPostalCodeProp, value->u.string);
        }
        value = getValueForAttribute(AB_attribCountry, values, numItems);
        if (value && value->u.string)
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if  (!t)
                t = addProp(vObj, VCAdrProp);
            addPropValue(t, VCCountryNameProp, value->u.string);
        }
        else
        {
            t = isAPropertyOf(vObj, VCAdrProp);
            if (!t)
            {
                t = addProp(vObj, VCAdrProp);
                addProp(t, VCDomesticProp);
            }
        }

        value = getValueForAttribute(AB_attribEmailAddress, values, numItems);
        if (value && value->u.string)
        {
            t = addPropValue(vObj, VCEmailAddressProp, value->u.string);
            addProp(t, VCInternetProp);
        }

        value = getValueForAttribute(AB_attribTitle, values, numItems);
        if (value && value->u.string)
            addPropValue(vObj, VCTitleProp, value->u.string);

        value = getValueForAttribute(AB_attribWorkPhone, values, numItems);
        if (value && value->u.string)
        {
            t = addPropValue(vObj, VCTelephoneProp, value->u.string);
            addProp(t, VCWorkProp);
        }

        value = getValueForAttribute(AB_attribFaxPhone, values, numItems);
        if (value && value->u.string)
        {
            t = addPropValue(vObj, VCTelephoneProp, value->u.string);
            addProp(t, VCFaxProp);
        }

        value = getValueForAttribute(AB_attribHomePhone, values, numItems);
        if (value && value->u.string)
        {
            t = addPropValue(vObj, VCTelephoneProp, value->u.string);
            addProp(t, VCHomeProp);
        }

        value = getValueForAttribute(AB_attribInfo, values, numItems);
        if (value && value->u.string)
            addPropValue(vObj, VCNoteProp, value->u.string);
        t = addProp(vObj, VCCooltalk);
        value = getValueForAttribute(AB_attribCoolAddress, values, numItems);
        if (value && value->u.string)
            addPropValue(t, VCCooltalkAddress, value->u.string);
        char * us = NULL;
        value = getValueForAttribute(AB_attribUseServer, values, numItems);
        if (value)
            us = PR_smprintf("%d", value->u.shortValue);
        if (us)
            addPropValue(t, VCUseServer, us);
        XP_FREEIF(us);

        value = getValueForAttribute(AB_attribHTMLMail, values, numItems);
        if (value && value->u.boolValue)
            addPropValue(vObj, VCUseHTML, "TRUE");
        else
            addPropValue(vObj, VCUseHTML, "FALSE");

        addPropValue(vObj, VCVersionProp, "2.1");
    }

    int len = 0;
    char * vCard = writeMemVObjects(0, &len, vObj);
    if (result)
        *result = vCard;
    else
        XP_FREEIF(vCard);
    return AB_SUCCESS;
}

/* ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ */
/* BEGIN MCCUSKER NETSCAPE GROMIT ADDITIONS */

#if defined(TRACE_orDEBUG_orPRINT_mccusker) && TRACE_orDEBUG_orPRINT_mccusker

#include <stdarg.h>
#include <time.h>
FILE* mccusker_gFile = 0;
time_t mccusker_gOpenTime = 0;
long mccusker_gFlushCounter = 0;
long mccusker_gDepth = 0;
char mccusker_gFileName[ 64 + 2 ];
long mccusker_gDelayedBegin = 0;

void mccusker_Begin()
{
    // ++mccusker_gDepth;
    ++mccusker_gDelayedBegin;
}

void mccusker_End()
{
    if ( mccusker_gDepth > 0 )
        --mccusker_gDepth;

    //if ( mccusker_gFile && (++mccusker_gFlushCounter & 7 ) == 0)
    //   fflush(mccusker_gFile);
}

void mccusker_Open()
{
#if defined(DEBUG_mccusker) && DEBUG_mccusker
    if ( !mccusker_gFile )
    {
        /* time_t time(time_t *timer);*/
        time_t now; time(&now);
        mccusker_gOpenTime = now;
        sprintf(mccusker_gFileName, "trace.%lX.dmc", (long) now);
        // mccusker_gFile = XP_FileOpen(filename, xpLDIFFile, XP_FILE_APPEND_BIN);
        mccusker_gFile = fopen(mccusker_gFileName, "a+");
        if ( mccusker_gFile )
        {
            mccusker_BeginTrace("mccusker_Open(%.32s) {\n", mccusker_gFileName);
        }
    }
#endif /* DEBUG_mccusker */
}

long mccusker_StrlenNoLinefeeds(char* s)
{
    register char c;
    register length = 0;
    
    while ( (c = *s++) != 0 )
    {
        if ( c == 0x0A )
            s[ -1 ] = (char) 0x0D;
        ++length;
    }
    return length;
}

void mccusker_Trace(const char* inFormat, ...)
{
#if defined(DEBUG_mccusker) && DEBUG_mccusker
    if ( !mccusker_gFile )
        mccusker_Open();

    if ( mccusker_gFile )
    {
        char buf[ 512 + 16 ];

        time_t now; time(&now);
        
        /* -----------123456789 123456789 123456789 123456789 123456789 123456789 */
        /* -----------12345678 10 345678 20 345678 30 345678 40 345678 50 345678 60 */
        sprintf(buf, "[%ld]                                                             ", 
            (long) (now - mccusker_gOpenTime));
        long depth = 10 + ( mccusker_gDepth * 2 );
        if ( depth > 64 )
            depth = 64;
        fwrite(buf, 1, depth, mccusker_gFile);

        va_list args;
        va_start(args,inFormat);
        PR_vsnprintf(buf, 512, inFormat, args);
        va_end(args);

        fwrite(buf, 1, mccusker_StrlenNoLinefeeds(buf), mccusker_gFile);
        
#ifndef NeoBIGCACHE_mccusker
        if ( (++mccusker_gFlushCounter & 0xFF ) == 0)
            fflush(mccusker_gFile);
#endif

        // XP_FileWrite(buf, strlen(buf), mccusker_gFile);
        // XP_FileFlush(mccusker_gFile);
    }
    if ( mccusker_gDelayedBegin > 0 )
    {
        mccusker_gDepth += mccusker_gDelayedBegin;
        mccusker_gDelayedBegin = 0;
    }
#endif /* DEBUG_mccusker */
}

void mccusker_Close()
{
#if defined(DEBUG_mccusker) && DEBUG_mccusker
    if ( mccusker_gFile )
    {
        mccusker_EndTrace("} /* mccusker_Close(%.32s) */\n", mccusker_gFileName);
        
        fclose(mccusker_gFile);
        // XP_FileClose(mccusker_gFile);
        mccusker_gFile = 0;
    }
#endif /* DEBUG_mccusker */
}

#if defined(DEBUG_mccusker) && DEBUG_mccusker
    class mccusker_StaticOpenFileTwiddler {
    public:
        mccusker_StaticOpenFileTwiddler();
        ~mccusker_StaticOpenFileTwiddler();
    };

    mccusker_StaticOpenFileTwiddler::mccusker_StaticOpenFileTwiddler()
    {
        mccusker_Trace("mccusker_StaticOpenFileTwiddler()\n");
    }
    mccusker_StaticOpenFileTwiddler::~mccusker_StaticOpenFileTwiddler()
    {
        if ( mccusker_gFile )
            mccusker_Trace("~mccusker_StaticOpenFileTwiddler()\n");
        mccusker_Close();
    }

    mccusker_StaticOpenFileTwiddler mccusker_gFileTwiddler; /* static instance */
#endif /* DEBUG_mccusker */

#else /* end TRACE_orDEBUG_orPRINT_mccusker */

void mccusker_Trace(const char* inFormat, ...)
{
    ((void) &inFormat);
}

void mccusker_Begin()
{
}

void mccusker_End()
{
}

#endif /* end TRACE_orDEBUG_orPRINT_mccusker */

/* END MCCUSKER NETSCAPE GROMIT ADDITIONS */
/* ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ ~~~~~ */
