/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini <mostafah@oeone.com>
 *                 Gary Frederick <gary.frederick@jsoft.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Alexandre Pauzies <apauzies@linagora.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* This file implements an XPCOM object which represents a calendar event. It provides access to individual 
fields of an event and performs calculations concerning its behaviour. The code for the eventDisplay object
which is an event with display information added to it is included here as well.
*/

#include <stdlib.h> // for atol

#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "plbase64.h"
#include "nsComponentManagerUtils.h"
#ifdef MOZ_MAIL_NEWS
#include "nsIAbCard.h"
#include "nsIMsgAttachment.h"
#endif
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"

#define strcasecmp strcmp

#define RECUR_NONE 0
#define RECUR_DAILY 1
#define RECUR_WEEKLY 2
#define RECUR_MONTHLY_MDAY 3
#define RECUR_MONTHLY_WDAY 4
#define RECUR_YEARLY 5

#define XPROP_SYNCID        "X-MOZILLA-SYNCID"
#define XPROP_LASTALARMACK  "X-MOZILLA-LASTALARMACK"
#define XPROP_RECURUNITS    "X-MOZILLA-RECUR-DEFAULT-UNITS"
#define XPROP_RECURINTERVAL "X-MOZILLA-RECUR-DEFAULT-INTERVAL"
#define XPROP_ALARMUNITS    "X-MOZILLA-ALARM-DEFAULT-UNITS"
#define XPROP_ALARMLENGTH   "X-MOZILLA-ALARM-DEFAULT-LENGTH"

#define DEFAULT_ALARM_UNITS "minutes"
#define DEFAULT_ALARM_LENGTH 15
#define DEFAULT_RECUR_UNITS "weeks"
#define DEFAULT_ALARMTRIGGER_RELATION ICAL_RELATED_START

extern oeIICalContainer *gContainer;

char *EmptyReturn() {
    return (char*) nsMemory::Clone( "", 1 );
}

icaltimetype ConvertFromPrtime( PRTime indate ) {
    icaltimetype outdate = icaltime_null_time();

    PRExplodedTime ext;
    PRInt64 indateinusec, usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_MUL( indateinusec, indate, usecpermsec );
    PR_ExplodeTime( indateinusec, PR_LocalTimeParameters, &ext);
 
    outdate.year = ext.tm_year;
    outdate.month = ext.tm_month + 1;
    outdate.day = ext.tm_mday;
    outdate.hour = ext.tm_hour;
    outdate.minute = ext.tm_min;
    outdate.second = ext.tm_sec;
    if( ext.tm_year <= 1969 && ext.tm_params.tp_dst_offset == 3600 )//Assume that daylight time saving was not in effect before 1970
        icaltime_adjust( &outdate, 0, -1, 0, 0 );

    return outdate;
}

PRTime ConvertToPrtime ( icaltimetype indate ) {
    PRExplodedTime ext;
    ext.tm_year = indate.year;
    ext.tm_month = indate.month - 1;
    ext.tm_mday = indate.day;
    ext.tm_hour = indate.hour;
    ext.tm_min = indate.minute;
    ext.tm_sec = indate.second;
    ext.tm_usec = 0;
    ext.tm_params.tp_gmt_offset = 0;
    ext.tm_params.tp_dst_offset = 0;

    PRTime result = PR_ImplodeTime( &ext );
    PR_ExplodeTime( result, PR_LocalTimeParameters, &ext);
    if( ext.tm_year <= 1969 && ext.tm_params.tp_dst_offset == 3600 ) //Assume that daylight time saving was not in effect before 1970
        ext.tm_params.tp_dst_offset = 0;
    ext.tm_year = indate.year;
    ext.tm_month = indate.month - 1;
    ext.tm_mday = indate.day;
    ext.tm_hour = indate.hour;
    ext.tm_min = indate.minute;
    ext.tm_sec = indate.second;
    ext.tm_usec = 0;
    result = PR_ImplodeTime( &ext );
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( result, result, usecpermsec );
    return result;
}

extern "C" {
#include "token.h"
}

void GenerateUUID(char *output) {
  uuid_state state;
  uuid_t uuid;

  create_uuid_state(&state);
  create_token(&state, &uuid);
  format_token(output, &uuid);
}

//////////////////////////////////////////////////
//   UTF-8 functions
//////////////////////////////////////////////////

// IsValidUTF scans a string and checks if it is UTF-compliant
// Most problems seem to come from 8-bit characters that are not ASCII with a value  > 0x80
PRBool
IsValidUTF8(const char* aBuffer)
{
    const char *c = aBuffer;
    const char *end = aBuffer + strlen(aBuffer);
    const char *lastchar = c;     // pre-initialize in case of 0-length buffer
    PRUint32 ucs2bytes = 0;
    PRUint32 utf8bytes = 0;
    while (c < end && *c) {
        lastchar = c;
        ucs2bytes++;

        if  ( (*c & 0x80) == 0x00 )     // ASCII
            c++;
        else if ( (*c & 0xE0) == 0xC0 ) // 2 byte
            c += 2;
        else if ( (*c & 0xF0) == 0xE0 ) // 3 byte
            c += 3;
        else if ( (*c & 0xF8) == 0xF0 ) // 4 byte
            c += 4;
        else if ( (*c & 0xFC) == 0xF8 ) // 5 byte
            c += 5;
        else if ( (*c & 0xFE) == 0xFC ) // 6 byte
            c += 6;
        else {
            break; // Otherwise we go into an infinite loop.  But what happens now?
        }
    }
    if (c != end) {
        #ifdef ICAL_DEBUG_ALL
            printf( "IsValidUTF8 Invalid UTF-8 string \"%s\"\n", aBuffer );
        #endif
        utf8bytes = 0;
         return false;
    }

    utf8bytes = c - aBuffer;
    return true;
}

// strForceUTF8 checks if a string is UTF-8 encoded, and if not assumes it made from  8-bit characters.
// If not UTF-8, all non-ASCII characters are replaced with '?'
// TODO: write or hook  a converter to convert non-ASCII to UTF-8
const char* strForceUTF8(const char * str)
{
#define CHAR_INVALID '?'
    const char* result = str;
    if (!IsValidUTF8( result ) ) {
        // make sure there are only ASCII chars in the string
        char *p = (char *)result;
        for( int i=0; '\0' != p[i]; i++ )
        if(!( (p[i] & 0x80) == 0x00) )
            p[i] = CHAR_INVALID;
    }
    return result;
}


#ifdef ICAL_DEBUG
int gEventCount = 0;
int gEventDisplayCount = 0;
#endif

//////////////////////////////////////////////////
//   ICalEvent Factory
//////////////////////////////////////////////////

/* Implementation file */
NS_IMPL_ISUPPORTS1(oeICalEventImpl, oeIICalEvent)

nsresult
NS_NewICalEvent( oeIICalEvent** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeICalEventImpl();
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeICalEventImpl::oeICalEventImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::oeICalEventImpl(): %d\n", ++gEventCount );
#endif
  /* member initializers and constructor code */
    nsresult rv;
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_start ))) {
        m_start = nsnull;
	}
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_end ))) {
        m_end = nsnull;
	}
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_stamp ))) {
        m_stamp = nsnull;
	}
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_recurend ))) {
        m_recurend = nsnull;
	}
    if( m_stamp ) {
        PRInt64 nowinusec = PR_Now();
        PRExplodedTime ext;
        PR_ExplodeTime( nowinusec, PR_GMTParameters, &ext);
        m_stamp->m_datetime.year = ext.tm_year;
        m_stamp->m_datetime.month = ext.tm_month + 1;
        m_stamp->m_datetime.day = ext.tm_mday;
        m_stamp->m_datetime.hour = ext.tm_hour;
        m_stamp->m_datetime.minute = ext.tm_min;
        m_stamp->m_datetime.second = ext.tm_sec;
        m_stamp->m_datetime.is_utc = true;
    }
    m_type = ICAL_VEVENT_COMPONENT;
    m_id = nsnull;
    m_title.SetIsVoid(true);
    m_description.SetIsVoid(true);
    m_location.SetIsVoid(true);
    m_category.SetIsVoid(true);
    m_url.SetIsVoid(true);
    m_priority = 0;
    m_method = 0;
    m_status = 0;
    m_isprivate = true;
    m_syncid = nsnull;
    m_allday = false;
    m_hasalarm = false;
    m_storeingmt = false;
    m_alarmlength = DEFAULT_ALARM_LENGTH;
    m_alarmtriggerrelation = DEFAULT_ALARMTRIGGER_RELATION;
    m_alarmemail = nsnull;
    m_inviteemail = nsnull;
    m_recurinterval = 1;
    m_recurcount = 0;
    m_recur = false;
    m_recurforever = true;
    m_alarmunits = nsnull;
    m_recurunits = nsnull;
    m_recurweekdays = 0;
    m_recurweeknumber = 0;
    m_lastalarmack = icaltime_null_time();
    m_lastmodified = icaltime_null_time();
    m_duration = icaldurationtype_null_duration();
    SetAlarmUnits( DEFAULT_ALARM_UNITS );
    SetRecurUnits( DEFAULT_RECUR_UNITS );
    SetSyncId( "" );
    NS_NewISupportsArray(getter_AddRefs(m_attachments));
    NS_NewISupportsArray(getter_AddRefs(m_contacts));
    m_calendar=nsnull;

    //Some defaults may have been changed in the prefs
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if ( NS_SUCCEEDED(rv) && prefBranch ) {
        nsXPIDLCString tmpstr;
        PRInt32 tmpint;
        rv = prefBranch->GetIntPref("calendar.alarms.onforevents", &tmpint);
        if (NS_SUCCEEDED(rv))
            m_hasalarm = tmpint;
        rv = prefBranch->GetBoolPref("calendar.dateformat.storeingmt", &tmpint);
        if (NS_SUCCEEDED(rv))
            m_storeingmt = tmpint;
        rv = prefBranch->GetIntPref("calendar.alarms.eventalarmlen", &tmpint);
        if (NS_SUCCEEDED(rv))
            m_alarmlength = tmpint;
        rv = prefBranch->GetCharPref("calendar.alarms.eventalarmunit", getter_Copies(tmpstr));
        if (NS_SUCCEEDED(rv))
            SetAlarmUnits( PromiseFlatCString( tmpstr ).get() );
        rv = prefBranch->GetCharPref("calendar.alarms.emailaddress", getter_Copies(tmpstr));
        if (NS_SUCCEEDED(rv)) {
            SetAlarmEmailAddress( PromiseFlatCString( tmpstr ).get() );
        }
    }
}

oeICalEventImpl::~oeICalEventImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::~oeICalEventImpl(): %d\n", --gEventCount );
#endif
    /* destructor code */
    if( m_id )
        nsMemory::Free( m_id );
    if( m_alarmunits )
        nsMemory::Free( m_alarmunits );
    if( m_alarmemail  )
        nsMemory::Free( m_alarmemail );
    if( m_inviteemail  )
        nsMemory::Free( m_inviteemail );
    if( m_recurunits  )
        nsMemory::Free( m_recurunits );
    if( m_syncid )
        nsMemory::Free( m_syncid );
  
    if( m_start )
        m_start->Release();
    if( m_end )
        m_end->Release();
    if( m_stamp )
        m_stamp->Release();
    if( m_recurend )
        m_recurend->Release();
    for( int i=0; i<m_snoozetimes.Count(); i++ ) {
        PRTime *snoozetimeptr = (PRTime*)(m_snoozetimes[i]);
        delete snoozetimeptr;
    }
    m_snoozetimes.Clear();
    RemoveAllExceptions();
}

NS_IMETHODIMP oeICalEventImpl::SetParent( oeIICal *calendar )
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::SetParent()\n");
#endif
    m_calendar = calendar;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetParent( oeIICal **calendar )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::GetParent()\n");
#endif
    *calendar = m_calendar;
    NS_ADDREF( *calendar );
    return NS_OK;
}

/* readonly attribute Componenttype type; */
NS_IMETHODIMP oeICalEventImpl::GetType(Componenttype *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetType() = " );
#endif
    *aRetVal= m_type;
#ifdef ICAL_DEBUG_ALL
    printf( "\"%d\"\n", *aRetVal );
#endif
    return NS_OK;
}

void oeICalEventImpl::SetType(Componenttype aNewVal) {
    m_type = aNewVal;
}

/* attribute string Id; */
NS_IMETHODIMP oeICalEventImpl::GetId(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetId() = " );
#endif
    if( m_id ) {
        *aRetVal= (char*) nsMemory::Clone( m_id, strlen(m_id)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= nsnull;
#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetId(const char *aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetId( %s )\n", aNewVal );
#endif
    if( m_id )
        nsMemory::Free( m_id );
    
    if( aNewVal )
        m_id= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_id = nsnull;

    return NS_OK;
}

bool oeICalEventImpl::matchId( const char *id ) {
    if( m_id )
        return ( strcmp( m_id, id ) == 0 );
    else
        return false;
}

/* attribute string Title; */
NS_IMETHODIMP oeICalEventImpl::GetTitle(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetTitle() = " );
#endif
    
    aRetVal =  m_title ;

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get()  );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetTitle(const nsACString& aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetTitle( %s )\n", PromiseFlatCString( aNewVal ).get()  );
#endif

    m_title = aNewVal;

    return NS_OK;
}

/* attribute string Description; */
NS_IMETHODIMP oeICalEventImpl::GetDescription(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetDescription() = " );
#endif
    aRetVal = m_description;
    
#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get()  );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetDescription(const nsACString& aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetDescription( %s )\n", PromiseFlatCString( aNewVal ).get() );
#endif

    m_description = aNewVal;
    
    return NS_OK;
}

/* attribute string Location; */
NS_IMETHODIMP oeICalEventImpl::GetLocation(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetLocation() = " );
#endif

    aRetVal= m_location;

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get()  );
#endif
   return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetLocation(const nsACString& aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetLocation( %s )\n", PromiseFlatCString( aNewVal ).get() );
#endif

    m_location = aNewVal;

    return NS_OK;
}

/* attribute string Category; */
NS_IMETHODIMP oeICalEventImpl::GetCategories(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetCategories() = " );
#endif
    
    aRetVal= m_category;

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get()  );
#endif
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetCategories(const nsACString& aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetCategories( %s )\n", PromiseFlatCString( aNewVal ).get() );
#endif
    m_category = aNewVal;

    return NS_OK;
}

/* attribute string Url; */
NS_IMETHODIMP oeICalEventImpl::GetUrl(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetUrl() = " );
#endif
    
    aRetVal= m_url;

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get()  );
#endif
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetUrl(const nsACString& aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetUrl( %s )\n", PromiseFlatCString( aNewVal ).get() );
#endif

    m_url = aNewVal;

    return NS_OK;
}

/* attribute boolean PrivateEvent; */
NS_IMETHODIMP oeICalEventImpl::GetPrivateEvent(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetPrivateEvent( ) = " );
#endif

    *aRetVal = m_isprivate;

#ifdef ICAL_DEBUG_ALL
    printf( "%d\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetPrivateEvent(PRBool aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetPrivateEvent( %d )\n", aNewVal );
#endif
    m_isprivate = aNewVal;
    return NS_OK;
}


/* attribute string SyncId; */
NS_IMETHODIMP oeICalEventImpl::GetSyncId(char * *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetSyncId()\n" );
#endif
    if( m_syncid ) {
        *aRetVal= (char*) nsMemory::Clone( m_syncid, strlen(m_syncid)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetSyncId(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetSyncId( %s )\n", aNewVal );
#endif
    if( m_syncid )
        nsMemory::Free( m_syncid );
    
    if( aNewVal )
        m_syncid= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_syncid = nsnull;
    return NS_OK;
}

/* attribute string Method; */
NS_IMETHODIMP oeICalEventImpl::GetMethod(eventMethodProperty *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetMethod() = " );
#endif
   *aRetVal= m_method;
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetMethod(eventMethodProperty aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetMethod( %d )\n", aNewVal );
#endif
    m_method= aNewVal;
    return NS_OK;
}

/* attribute string Status; */
NS_IMETHODIMP oeICalEventImpl::GetStatus(eventStatusProperty *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetStatus() = " );
#endif
   *aRetVal= m_status;
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetStatus(eventStatusProperty aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetStatus( %d )\n", aNewVal );
#endif
    m_status= aNewVal;
    return NS_OK;
}

/* attribute short Priority; */
NS_IMETHODIMP oeICalEventImpl::GetPriority(PRInt16 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetPriority() = \n" );
#endif
    *aRetVal = m_priority;
#ifdef ICAL_DEBUG_ALL
    printf( "%d\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetPriority(PRInt16 aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetPriority( %d )\n", aNewVal );
#endif
    m_priority = aNewVal;
    return NS_OK;
}

/* attribute boolean AllDay; */
NS_IMETHODIMP oeICalEventImpl::GetAllDay(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetAllDay()\n" );
#endif
    *aRetVal = m_allday;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetAllDay(PRBool aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetAllDay( %d )\n", aNewVal );
#endif
    m_allday = aNewVal;
    return NS_OK;
}

/* attribute boolean Alarm; */
NS_IMETHODIMP oeICalEventImpl::GetAlarm(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetAlarm()\n" );
#endif
    *aRetVal = m_hasalarm;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarm(PRBool aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetAlarm()\n" );
#endif
    m_hasalarm = aNewVal;
    return NS_OK;
}

/* attribute string AlarmUnits; */
NS_IMETHODIMP oeICalEventImpl::GetAlarmUnits(char * *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetAlarmUnits()\n" );
#endif
    if( m_alarmunits ) {
        *aRetVal= (char*) nsMemory::Clone( m_alarmunits, strlen(m_alarmunits)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarmUnits(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetAlarmUnits( %s )\n", aNewVal );
#endif
    if( m_alarmunits )
        nsMemory::Free( m_alarmunits );
    
    if( aNewVal )
        m_alarmunits= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_alarmunits = nsnull;
    return NS_OK;
}

/* attribute long AlarmLength; */
NS_IMETHODIMP oeICalEventImpl::GetAlarmLength(PRUint32 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetAlarmLength()\n" );
#endif
    *aRetVal = m_alarmlength;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarmLength(PRUint32 aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetAlarmLength(%ul)\n", aNewVal );
#endif
    m_alarmlength = aNewVal;
    return NS_OK;
}

/* attribute string AlarmEmailAddress; */
NS_IMETHODIMP oeICalEventImpl::GetAlarmEmailAddress(char * *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetAlarmEmailAddres()\n" );
#endif
    if( m_alarmemail ) {
        *aRetVal= (char*) nsMemory::Clone( m_alarmemail, strlen(m_alarmemail)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarmEmailAddress(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetAlarmEmailAddres( %s )\n", aNewVal );
#endif
    if( m_alarmemail )
        nsMemory::Free( m_alarmemail );
    
    if( aNewVal )
        m_alarmemail= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_alarmemail = nsnull;
    return NS_OK;
}

/* attribute string InviteEmailAddress; */
NS_IMETHODIMP oeICalEventImpl::GetInviteEmailAddress(char * *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetInviteEmailAddres()\n" );
#endif
    if( m_inviteemail ) {
        *aRetVal= (char*) nsMemory::Clone( m_inviteemail, strlen(m_inviteemail)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
   return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetInviteEmailAddress(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetInviteEmailAddres()\n" );
#endif
    if( m_inviteemail )
        nsMemory::Free( m_inviteemail );
    
    if( aNewVal )
        m_inviteemail= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_inviteemail = nsnull;
    return NS_OK;
}

/* attribute RecurInterval; */
NS_IMETHODIMP oeICalEventImpl::GetRecurInterval(PRUint32 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurInterval()\n" );
#endif
    *aRetVal = m_recurinterval;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurInterval(PRUint32 aNewVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurInterval()\n" );
#endif
    m_recurinterval = aNewVal;
    return NS_OK;
}

/* attribute RecurCount; */
NS_IMETHODIMP oeICalEventImpl::GetRecurCount(PRUint32 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurCount()\n" );
#endif
    *aRetVal = m_recurcount;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurCount(PRUint32 aNewVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurCount()\n" );
#endif
    m_recurcount = aNewVal;
    return NS_OK;
}

/* attribute string RecurUnits; */

NS_IMETHODIMP oeICalEventImpl::GetRecurUnits(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurUnits()\n" );
#endif
    if( m_recurunits ) {
        *aRetVal= (char*) nsMemory::Clone( m_recurunits, strlen(m_recurunits)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurUnits(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurUnits( %s )\n", aNewVal );
#endif
    if( m_recurunits )
        nsMemory::Free( m_recurunits );
    
    if( aNewVal )
        m_recurunits= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_recurunits = nsnull;
    return NS_OK;
}

/* attribute boolean Recur; */

NS_IMETHODIMP oeICalEventImpl::GetRecur(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecur()\n" );
#endif
    *aRetVal = m_recur;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecur(PRBool aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecur()\n" );
#endif
    m_recur = aNewVal;
    return NS_OK;
}

/* attribute boolean RecurForever; */

NS_IMETHODIMP oeICalEventImpl::GetRecurForever(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurForever() = " );
#endif
    *aRetVal = m_recurforever;
#ifdef ICAL_DEBUG_ALL
    printf( "%d\n", *aRetVal );
#endif
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurForever(PRBool aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurForever()\n" );
#endif
    m_recurforever = aNewVal;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetLastModified(PRTime *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetLastModified()\n" );
#endif
    if( icaltime_is_null_time( m_lastmodified ) )
        return NS_ERROR_NOT_INITIALIZED;

    *aRetVal = ConvertToPrtime( m_lastmodified );
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::UpdateLastModified()
{
#ifdef ICAL_DEBUG_ALL
    printf( "UpdateLastModified()\n" );
#endif

    PRInt64 nowinusec = PR_Now();
    PRExplodedTime ext;
    PR_ExplodeTime( nowinusec, PR_GMTParameters, &ext);
    m_lastmodified = icaltime_null_time();
    m_lastmodified.year = ext.tm_year;
    m_lastmodified.month = ext.tm_month + 1;
    m_lastmodified.day = ext.tm_mday;
    m_lastmodified.hour = ext.tm_hour;
    m_lastmodified.minute = ext.tm_min;
    m_lastmodified.second = ext.tm_sec;
    m_lastmodified.is_utc = true;

    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetLastAlarmAck(PRTime *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetLastAlarmAck()\n" );
#endif
    *aRetVal = ConvertToPrtime( m_lastalarmack );
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetLastAlarmAck(PRTime aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetLastAlarmAck()\n" );
#endif

    m_lastalarmack = ConvertFromPrtime( aNewVal );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetNextRecurrence( PRTime begin, PRTime *retval, PRBool *isvalid ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::GetNextRecurrence()\n" );
#endif
    *isvalid = false;
    icaltimetype begindate,result;
    begindate = ConvertFromPrtime( begin );
    result = GetNextRecurrence( begindate, nsnull );
    result.is_date = false;
    if( icaltime_is_null_time( result ) )
        return NS_OK;
    *retval = ConvertToPrtime( result );
    *isvalid = true;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetPreviousOccurrence( PRTime beforethis, PRTime *retval, PRBool *isvalid ) {
#ifdef ICAL_DEBUG_ALL
    printf( "GetPreviousOccurrence()\n" );
#endif
    //for non recurring events
    *isvalid = false;
    if( !m_recur ) {
        icaltimetype before = ConvertFromPrtime( beforethis );
        if( icaltime_compare( m_start->m_datetime, before ) < 0 ) {
            *retval = ConvertToPrtime( m_start->m_datetime );
            *isvalid = true;
        }
        return NS_OK;
    }

    //for recurring events
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::GetPreviousOccurrence() failed!\n" );
        #endif
        return NS_OK;
    }

    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        struct icalrecurrencetype recur = icalproperty_get_rrule(prop);
//        printf("#### %s\n",icalrecurrencetype_as_string(&recur));
        icalrecur_iterator* ritr = icalrecur_iterator_new(recur,m_start->m_datetime);
        struct icaltimetype next;
        for(next = icalrecur_iterator_next(ritr);
            !icaltime_is_null_time(next);
            next = icalrecur_iterator_next(ritr)){

            next.is_date = false;
            next = icaltime_normalize( next );

//            printf( "recur: %d-%d-%d %d:%d:%d\n" , next.year, next.month, next.day, next.hour, next.minute, next.second );
            
            //quick fix for the recurrence getting out of the end of the month into the next month
            //like 31st of each month but when you get February
            if( recur.freq == ICAL_MONTHLY_RECURRENCE && !m_recurweeknumber && next.day != m_start->m_datetime.day) {
//#ifdef ICAL_DEBUG
//                printf( "Wrong day in month\n" );
//#endif
//                continue;
                next.day = 0;
                next = icaltime_normalize( next );
            }
            PRTime nextinms = ConvertToPrtime( next );
            if( LL_CMP(nextinms, < ,beforethis) && !IsExcepted( nextinms ) ) {
//                printf( "Result: %d-%d-%d %d:%d\n" , next.year, next.month, next.day, next.hour, next.minute );
                *retval = nextinms;
                *isvalid = true;
            }
            if( LL_CMP(nextinms, >= ,beforethis) ) {
                break;
            }
        }
        icalrecur_iterator_free(ritr);
    }

    icalcomponent_free( vcalendar );
    return NS_OK;
}

icaltimetype oeICalEventImpl::GetNextRecurrence( icaltimetype begin, bool *isbeginning ) {
    icaltimetype result = icaltime_null_time();

    if( isbeginning ) {
        *isbeginning = true;
    }

    if( icaltime_compare( m_start->m_datetime , begin ) > 0 ) {
        if( !m_recur )
            return m_start->m_datetime;
        PRTime nextinms = ConvertToPrtime( m_start->m_datetime );
        if( !IsExcepted( nextinms ) )
            return m_start->m_datetime;
    }

    //for non recurring events
    if( !m_recur ) {

        if( icaltime_compare( m_end->m_datetime , begin ) <= 0 )
            return result;

        struct icaltimetype nextday = begin;
        nextday.hour = 0; nextday.minute = 0; nextday.second = 0;
        icaltime_adjust( &nextday, 1, 0, 0, 0 );
        if( icaltime_compare( nextday, m_end->m_datetime ) < 0 ) {
            struct icaltimetype afternextday = nextday;
            icaltime_adjust( &afternextday, 1, 0, 0, 0 );
            if( icaltime_compare( afternextday, m_end->m_datetime ) < 0 ) {
                nextday.is_date = true;
            }
            if( isbeginning ) {
                *isbeginning = false;
            }
            return nextday;
        }
        return result;
    }

    //for recurring events
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::GetNextRecurrence() failed!\n" );
        #endif
        return result;
    }

    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        struct icaltimetype next,nextpropagation=icaltime_null_time();
        struct icalrecurrencetype recur = icalproperty_get_rrule(prop);
        //printf("#### %s\n",icalrecurrencetype_as_string(&recur));
        icalrecur_iterator* ritr = icalrecur_iterator_new(recur,m_start->m_datetime);
        bool nextpropagationisdate = false;
        for(next = icalrecur_iterator_next(ritr);
            !icaltime_is_null_time(next);
            next = icalrecur_iterator_next(ritr)){

            next.is_date = false;
            next = icaltime_normalize( next );

            //printf( "recur: %d-%d-%d %d:%d:%d\n" , next.year, next.month, next.day, next.hour, next.minute, next.second );
            
            //quick fix for the recurrence getting out of the end of the month into the next month
            //like 31st of each month but when you get February
            if( recur.freq == ICAL_MONTHLY_RECURRENCE && !m_recurweeknumber && next.day != m_start->m_datetime.day) {
                //#ifdef ICAL_DEBUG
                //printf( "Wrong day in month\n" );
                //#endif
                //continue;
                next.day = 0;
                next = icaltime_normalize( next );
            }

            if( icaltime_compare( next, begin ) > 0 ) {
                PRTime nextinms = ConvertToPrtime( next );
                 if( !IsExcepted( nextinms ) ) {
                    //printf( "Result: %d-%d-%d %d:%d\n" , next.year, next.month, next.day, next.hour, next.minute );
                    result = next;
                    break;
                 }
            }

            if( icaltime_is_null_time( nextpropagation ) ) {
                struct icaldurationtype eventlength = GetLength();
                struct icaltimetype end = icaltime_add( next, eventlength );

                if( icaltime_compare( end , begin ) <= 0 )
                    continue;

                struct icaltimetype nextday = next;
                nextday.hour = 0; nextday.minute = 0; nextday.second = 0;
                icaltime_adjust( &nextday, 1, 0, 0, 0 );
                if( icaltime_compare( nextday , begin ) > 0 && icaltime_compare( nextday, end ) < 0 ) {
                    PRTime nextdayinms = ConvertToPrtime( nextday );
                     if( !IsExcepted( nextdayinms ) ) {
                        nextpropagation = nextday;
                        struct icaltimetype afternextday = nextday;
                        icaltime_adjust( &afternextday, 1, 0, 0, 0 );
                        if( icaltime_compare( afternextday, end ) < 0 ) {
                            nextpropagationisdate = true;
                        }
                     }
                }
            }
        }
        icalrecur_iterator_free(ritr);
        if( !icaltime_is_null_time( result ) ) {
            if( !icaltime_is_null_time( nextpropagation ) ) {
                if( icaltime_compare( nextpropagation , result ) < 0 ) {
                    result = nextpropagation;
                    result.is_date = nextpropagationisdate;
                    if( isbeginning ) {
                        *isbeginning = false;
                    }
                }
            }
        } else if( !icaltime_is_null_time( nextpropagation ) ) {
            result = nextpropagation;
            result.is_date = nextpropagationisdate;
            if( isbeginning ) {
                *isbeginning = false;
            }
        }
    }

    icalcomponent_free( vcalendar );

    return result;
}

icaltimetype oeICalEventImpl::GetPreviousOccurrence( icaltimetype beforethis ) {
    icaltimetype result = icaltime_null_time();
    PRTime beforethisinms = ConvertToPrtime( beforethis );
    PRTime resultinms;
    PRBool isvalid;
    GetPreviousOccurrence( beforethisinms ,&resultinms, &isvalid );
    if( !isvalid )
        return result;

    result = ConvertFromPrtime( resultinms );
    return result;
}

icaltimetype oeICalEventImpl::GetNextAlarmTime( icaltimetype begin ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::GetNextAlarmTime()\n" );
#endif
    icaltimetype result = icaltime_null_time();

    if( !m_hasalarm )
        return result;

    // Check for related to the start, and no start set
    // or a start year set to -1, which might occur for invalid start times
    // due to timezone weirdness
    PRBool isSet = PR_FALSE;
    m_start->GetIsSet(&isSet);
    PRInt16 year;
    m_start->GetYear(&year);
    if( m_alarmtriggerrelation == ICAL_RELATED_START && (!isSet || year == -1) )
        return result;

    icaltimetype starting = begin;

    if( !icaltime_is_null_time( m_lastalarmack ) && icaltime_compare( begin, m_lastalarmack ) < 0 )
        starting = m_lastalarmack;

    icaltimetype checkloop = starting;
    do {
        checkloop = GetNextRecurrence( checkloop, nsnull );
        checkloop.is_date = false;
        result = checkloop;
        if( icaltime_is_null_time( checkloop ) ) {
            break;
        }
        result = CalculateAlarmTime( result );
    } while ( icaltime_compare( starting, result ) >= 0 );

    for( int i=0; i<m_snoozetimes.Count(); i++ ) {
        icaltimetype snoozetime = ConvertFromPrtime( *(PRTime*)(m_snoozetimes[i]) );
        if( icaltime_is_null_time( result ) || icaltime_compare( snoozetime, result ) < 0 ) {
            if ( icaltime_compare( snoozetime, starting ) > 0 ) {
                result = snoozetime;
            }
        }
    }
    return result;
}

icaltimetype oeICalEventImpl::CalculateAlarmTime( icaltimetype date ) {
    icaltimetype result = date;
    if( strcasecmp( m_alarmunits, "days" ) == 0 )
        icaltime_adjust( &result, -(signed long)m_alarmlength, 0, 0, 0 );
    else if( strcasecmp( m_alarmunits, "hours" ) == 0 )
        icaltime_adjust( &result, 0, -(signed long)m_alarmlength, 0, 0 );
    else
        icaltime_adjust( &result, 0, 0, -(signed long)m_alarmlength, 0 );

    //Add the length to the final result if alarm trigger is relative to end of event
    if( m_alarmtriggerrelation == ICAL_RELATED_END )
        result = icaltime_add( result, GetLength() );

    return result;
}

icaltimetype oeICalEventImpl::CalculateEventTime( icaltimetype alarmtime ) {
    icaltimetype result = alarmtime;
    if( strcasecmp( m_alarmunits, "days" ) == 0 )
        icaltime_adjust( &result, (signed long)m_alarmlength, 0, 0, 0 );
    else if( strcasecmp( m_alarmunits, "hours" ) == 0 )
        icaltime_adjust( &result, 0, (signed long)m_alarmlength, 0, 0 );
    else
        icaltime_adjust( &result, 0, 0, (signed long)m_alarmlength, 0 );

    return result;
}

icaldurationtype oeICalEventImpl::GetLength() {

    if( !icaldurationtype_is_null_duration( m_duration ) )
        return m_duration;

    return icaltime_subtract( m_end->m_datetime, m_start->m_datetime );;
}

NS_IMETHODIMP oeICalEventImpl::GetStart(oeIDateTime * *start)
{
    *start = m_start;
    NS_ADDREF(*start);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetEnd(oeIDateTime * *end)
{
    *end = m_end;
    NS_ADDREF(*end);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetStamp(oeIDateTime * *stamp)
{
    *stamp = m_stamp;
    NS_ADDREF(*stamp);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetRecurEnd(oeIDateTime * *recurend)
{
    *recurend = m_recurend;
    NS_ADDREF(*recurend);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetRecurWeekdays(PRInt16 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurWeekdays()\n" );
#endif
    *aRetVal = m_recurweekdays;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurWeekdays(PRInt16 aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurWeekdays()\n" );
#endif
    m_recurweekdays = aNewVal;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetRecurWeekNumber(PRInt16 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetRecurWeekNumber()\n" );
#endif
    *aRetVal = m_recurweeknumber;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurWeekNumber(PRInt16 aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetRecurWeekNumber()\n" );
#endif
    m_recurweeknumber = aNewVal;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetIcalString(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetIcalString() = " );
#endif
    
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::GetIcalString() failed!\n" );
        #endif
        return NS_OK;
    }

    char *str = icalcomponent_as_ical_string( vcalendar );
    if( str ) {
		aRetVal = str;
//        *aRetVal= (char*) nsMemory::Clone( str, strlen(str)+1);
    } else
        aRetVal.Truncate();
    icalcomponent_free( vcalendar );

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get() );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::ParseIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "ParseIcalString( %s )\n", PromiseFlatCString( aNewVal ).get() );
#endif
    
    *aRetVal = false;
    icalcomponent *comp = icalparser_parse_string( PromiseFlatCString( aNewVal ).get() );
    if( comp ) {
        if( ParseIcalComponent( comp ) )
            *aRetVal = true;
        icalcomponent_free( comp );
    }
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::AddException( PRTime exdate )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::AddException()\n" );
#endif
    icaltimetype tmpexdate = ConvertFromPrtime( exdate );
    tmpexdate.hour = 0;
    tmpexdate.minute = 0;
    tmpexdate.second = 0;
    exdate = ConvertToPrtime( tmpexdate );
    PRTime *newexdate = new PRTime;
    if (!newexdate)
      return NS_ERROR_OUT_OF_MEMORY;
    *newexdate = exdate;
    m_exceptiondates.AppendElement( newexdate );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::RemoveAllExceptions()
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::RemoveAllExceptions()\n" );
#endif
    for( int i=0; i<m_exceptiondates.Count(); i++ ) {
        PRTime *exceptiondateptr = (PRTime*)(m_exceptiondates[i]);
        delete exceptiondateptr;
    }
    m_exceptiondates.Clear();
    return NS_OK;
}

NS_IMETHODIMP
oeICalEventImpl::GetExceptions(nsISimpleEnumerator **datelist )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::GetExceptions()\n" );
#endif
    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator();
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    int count=0;
    bool found;
    PRTime current,lastadded,minimum;

    do {
        found = false;
        for( int i=0; i<m_exceptiondates.Count(); i++ ) {
            current = *(PRTime *)(m_exceptiondates[i]);
            if( !count ) {
                if( i == 0 ) {
                    minimum = current;
                    found = true;
                } else if ( LL_CMP( current, < , minimum ) ) {
                    minimum = current;
                    found = true;
                }
            } else if ( LL_CMP( current, >, lastadded ) ) {
                if( LL_CMP( minimum, ==, lastadded ) ) {
                    minimum = current;
                    found = true;
                }
                else if( LL_CMP( current, <, minimum ) ) {
                    minimum = current;
                    found = true;
                }
            }
        }

        if( found ) {
            dateEnum->AddDate( minimum );
            count++;
            lastadded = minimum;
        }
    } while ( found );

    // bump ref count
    return dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
}

bool oeICalEventImpl::IsExcepted( PRTime date ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::IsExcepted() = " );
#endif
    icaltimetype tmpexdate = ConvertFromPrtime( date );
    tmpexdate.hour = 0;
    tmpexdate.minute = 0;
    tmpexdate.second = 0;
    date = ConvertToPrtime( tmpexdate );
    
    bool result = false;
    for( int i=0; i<m_exceptiondates.Count(); i++ ) {
        if( *(PRTime *)(m_exceptiondates[i]) == date ) {
            result = true;
            break;
        }
    }
#ifdef ICAL_DEBUG_ALL
    printf( "%d\n", result );
#endif
    return result;
}

NS_IMETHODIMP oeICalEventImpl::SetSnoozeTime( PRTime snoozetime )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::SetSnoozeTime()\n" );
#endif
    icaltimetype tmpdate = ConvertFromPrtime( snoozetime );
    snoozetime = icaltime_as_timet( tmpdate );
    PRTime *snoozetimeinms = new PRTime;
    if (!snoozetimeinms)
      return NS_ERROR_OUT_OF_MEMORY;
    *snoozetimeinms = snoozetime * 1000;
    m_snoozetimes.AppendElement( snoozetimeinms );
    PRTime nowinms = time( nsnull );
    nowinms *= 1000;
    m_lastalarmack = ConvertFromPrtime( nowinms );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::Clone( oeIICalEvent **ev )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::Clone()\n" );
#endif
    nsresult rv;
    oeICalEventImpl *icalevent =nsnull;
    if( NS_FAILED( rv = NS_NewICalEvent( (oeIICalEvent**) &icalevent ) ) ) {
        return rv;
    }
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::Clone() failed!\n" );
        #endif
        icalevent->Release();
        return NS_OK;
    }
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    if( !(icalevent->ParseIcalComponent( vevent )) ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::Clone() failed.\n" );
        #endif
        icalevent->Release();
        return NS_OK;
    }
    *ev = icalevent;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetAttachmentsArray(nsISupportsArray * *aAttachmentsArray)
{
    NS_ENSURE_ARG_POINTER(aAttachmentsArray);
    *aAttachmentsArray = m_attachments;
    NS_IF_ADDREF(*aAttachmentsArray);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::AddAttachment(nsIMsgAttachment *attachment)
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::AddAttachment()\n" );
#endif
#ifdef MOZ_MAIL_NEWS
    PRUint32 i;
    PRUint32 attachmentCount = 0;
    m_attachments->Count(&attachmentCount);

    //Don't add twice the same attachment.
    nsCOMPtr<nsIMsgAttachment> element;
    PRBool sameUrl;
    for (i = 0; i < attachmentCount; i ++)
    {
        m_attachments->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
        if (element)
        {
            element->EqualsUrl(attachment, &sameUrl);
            if (sameUrl)
                return NS_OK;
        }
    }

    return m_attachments->InsertElementAt(attachment, attachmentCount);
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP oeICalEventImpl::RemoveAttachment(nsIMsgAttachment *attachment)
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::RemoveAttachment()\n" );
#endif
#ifdef MOZ_MAIL_NEWS
    PRUint32 i;
    PRUint32 attachmentCount = 0;
    m_attachments->Count(&attachmentCount);

    nsCOMPtr<nsIMsgAttachment> element;
    PRBool sameUrl;
    for (i = 0; i < attachmentCount; i ++)
    {
        m_attachments->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
        if (element)
        {
            element->EqualsUrl(attachment, &sameUrl);
            if (sameUrl)
            {
                m_attachments->DeleteElementAt(i);
                break;
            }
        }
    }

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP oeICalEventImpl::RemoveAttachments()
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::RemoveAttachments()\n" );
#endif
    PRUint32 i;
    PRUint32 attachmentCount = 0;
    m_attachments->Count(&attachmentCount);

    for (i = 0; i < attachmentCount; i ++)
        m_attachments->DeleteElementAt(0);

    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetContactsArray(nsISupportsArray * *aContactsArray)
{
    NS_ENSURE_ARG_POINTER(aContactsArray);
    *aContactsArray = m_contacts;
    NS_IF_ADDREF(*aContactsArray);
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::AddContact(nsIAbCard *contact)
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::AddContact()\n" );
#endif
#ifdef MOZ_MAIL_NEWS
    PRUint32 i;
    PRUint32 contactCount = 0;
    m_contacts->Count(&contactCount);

    //Don't add twice the same contact.
    nsCOMPtr<nsIAbCard> element;
    PRBool samecontact;
    for (i = 0; i < contactCount; i ++)
    {
        m_contacts->QueryElementAt(i, NS_GET_IID(nsIAbCard), getter_AddRefs(element));
        if (element)
        {
            element->Equals(contact, &samecontact);
            if (samecontact)
                return NS_OK;
        }
    }

    return m_contacts->InsertElementAt(contact, contactCount);
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP oeICalEventImpl::RemoveContact(nsIAbCard *contact)
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::RemoveContact()\n" );
#endif
#ifdef MOZ_MAIL_NEWS
    PRUint32 i;
    PRUint32 contactCount = 0;
    m_contacts->Count(&contactCount);

    nsCOMPtr<nsIAbCard> element;
    PRBool samecontact;
    for (i = 0; i < contactCount; i ++)
    {
        m_contacts->QueryElementAt(i, NS_GET_IID(nsIAbCard), getter_AddRefs(element));
        if (element)
        {
            element->Equals(contact, &samecontact);
            if (samecontact)
            {
                m_contacts->DeleteElementAt(i);
                break;
            }
        }
    }

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP oeICalEventImpl::RemoveContacts()
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::RemoveContacts()\n" );
#endif
#ifdef MOZ_MAIL_NEWS
    PRUint32 i;
    PRUint32 contactCount = 0;
    m_contacts->Count(&contactCount);

    for (i = 0; i < contactCount; i ++)
        m_contacts->DeleteElementAt(0);

    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP oeICalEventImpl::SetDuration(PRBool is_negative, PRUint16 weeks, PRUint16 days, PRUint16 hours, PRUint16 minutes, PRUint16 seconds)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::SetDuration()\n" );
#endif
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalEventImpl::GetDuration(PRBool *is_negative, PRUint16 *weeks, PRUint16 *days, PRUint16 *hours, PRUint16 *minutes, PRUint16 *seconds)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::GetDuration()\n" );
#endif
    return NS_ERROR_NOT_IMPLEMENTED;
}

void oeICalEventImpl::ChopAndAddEventToEnum( struct icaltimetype startdate, nsISimpleEnumerator **eventlist, 
                                               bool isallday, bool isbeginning ) {

    nsCOMPtr<oeEventEnumerator> eventEnum;
    eventEnum = (oeEventEnumerator *)*eventlist;

    oeIICalEventDisplay* eventDisplay;
    nsresult rv = NS_NewICalEventDisplay( this, &eventDisplay );
    if( NS_FAILED( rv ) ) {
    #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::ChopAndAddEventToEnum() : WARNING Cannot create oeIICalEventDisplay instance: %x\n", rv );
    #endif
        return;
    }
    eventEnum->AddEvent( eventDisplay );

    PRTime startdateinms = ConvertToPrtime( startdate );
    eventDisplay->SetDisplayDate( startdateinms );

    struct icaltimetype endofday = startdate;
    endofday.hour = 23; endofday.minute = 59; endofday.second = 59;

    PRTime enddateinms;
    if( isallday ) {
        enddateinms = ConvertToPrtime( endofday );
        eventDisplay->SetDisplayEndDate( enddateinms );
    } else {
        if( isbeginning ) {
            struct icaldurationtype eventlength = GetLength();
            struct icaltimetype eventenddate = icaltime_add( startdate, eventlength );

            if( icaltime_compare( endofday, eventenddate ) < 0 ) {
                enddateinms = ConvertToPrtime( endofday );
            } else {
                enddateinms = ConvertToPrtime( eventenddate );
            }
        } else {
            struct icaltimetype eventenddate = endofday;
            eventenddate.hour = m_end->m_datetime.hour;
            eventenddate.minute = m_end->m_datetime.minute;
            eventenddate.second = m_end->m_datetime.second;
            enddateinms = ConvertToPrtime( eventenddate );
        }
        eventDisplay->SetDisplayEndDate( enddateinms );
    }
    NS_RELEASE( eventDisplay );
}

bool oeICalEventImpl::ParseIcalComponent( icalcomponent *comp )
{
#ifdef ICAL_DEBUG_ALL
    printf( "ParseIcalComponent()\n" );
#endif

    icalcomponent *vevent=nsnull;
    icalcomponent_kind kind = icalcomponent_isa( comp );

    if( kind == ICAL_VCALENDAR_COMPONENT ) {
	    vevent = icalcomponent_get_first_component( comp , ICAL_VEVENT_COMPONENT );
        if ( !vevent ) {
            icalcomponent_get_first_component( comp , ICAL_VTODO_COMPONENT );
        }
    }
    else if( kind == ICAL_VEVENT_COMPONENT )
        vevent = comp;
    else if( kind == ICAL_VTODO_COMPONENT )
        vevent = comp;

    if ( !vevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::ParseIcalComponent() failed: vevent is NULL!\n" );
        #endif
        return false;
    }

    m_type = icalcomponent_isa( vevent );

    const char *tmpstr;
    //id    
    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    if ( prop ) {
        tmpstr = icalproperty_get_uid( prop );
        SetId( tmpstr );
    } else {
        ReportError( oeIICalError::CAL_WARN, oeIICalError::UID_NOT_FOUND, "oeICalEventImpl::ParseIcalComponent() : Warning UID not found! Assigning new one." );
        char uidstr[40];
        GenerateUUID( uidstr );
        SetId( uidstr );
        prop = icalproperty_new_uid( uidstr );
        icalcomponent_add_property( vevent, prop );
    }

    //method
    if( kind == ICAL_VCALENDAR_COMPONENT ) {
       prop = icalcomponent_get_first_property( comp, ICAL_METHOD_PROPERTY );
       if ( prop != 0) {
	   eventMethodProperty tmpMethodProp;
           tmpMethodProp = icalproperty_get_method( prop );
           SetMethod( tmpMethodProp );
       } else  if ( m_method ) {
	   m_method = 0;
       }	    
    }

    //status
   prop = icalcomponent_get_first_property( vevent, ICAL_STATUS_PROPERTY );
   if ( prop != 0) {
       eventStatusProperty tmpStatusProp;
       tmpStatusProp = icalproperty_get_status( prop );
       SetStatus( tmpStatusProp );
   } else  if ( m_status ) {
       m_status = 0;
   }	    

    //title
    prop = icalcomponent_get_first_property( vevent, ICAL_SUMMARY_PROPERTY );
    if ( prop != 0 && (tmpstr = icalproperty_get_summary( prop ) ) ) {
        SetTitle( nsDependentCString( strForceUTF8( tmpstr ) ) );
    } else if( !m_title.IsEmpty() ) {
        m_title.Truncate();
    }

    //description
    prop = icalcomponent_get_first_property( vevent, ICAL_DESCRIPTION_PROPERTY );
    if ( prop != 0 && (tmpstr = icalproperty_get_description( prop ) ) ) {
        SetDescription( nsDependentCString( strForceUTF8( tmpstr ) ) );
    } else if( !m_description.IsEmpty() ) {
        m_description.Truncate();
    }

    //location
    prop = icalcomponent_get_first_property( vevent, ICAL_LOCATION_PROPERTY );
    if ( prop != 0) {
        tmpstr = icalproperty_get_location( prop );
        SetLocation( nsDependentCString( strForceUTF8( tmpstr ) ) );
    } else if( !m_location.IsEmpty() ) {
        m_location.Truncate();
    }

    //category
    prop = icalcomponent_get_first_property( vevent, ICAL_CATEGORIES_PROPERTY );
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_categories( prop );
        SetCategories( nsDependentCString( strForceUTF8( tmpstr ) ) );
    } else if( !m_category.IsEmpty() ) {
        m_category.Truncate();
    }

    //url
    prop = icalcomponent_get_first_property( vevent, ICAL_URL_PROPERTY );
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_url( prop );
        SetUrl( nsDependentCString( strForceUTF8( tmpstr ) ) );
    } else if( !m_url.IsEmpty() ) {
        m_url.Truncate();
    }

    //priority
    prop = icalcomponent_get_first_property( vevent, ICAL_PRIORITY_PROPERTY );
    if ( prop != 0) {
        m_priority = icalproperty_get_priority( prop );
    } else {
        m_priority = 0;
    }                             

    //isprivate
    prop = icalcomponent_get_first_property( vevent, ICAL_CLASS_PROPERTY );
    if ( prop != 0) {
        icalproperty_class tmpcls = icalproperty_get_class( prop );
        if( tmpcls == ICAL_CLASS_PUBLIC )
            m_isprivate= false;
        else
            m_isprivate= true;
    } else
        m_isprivate= false;

    //syncid
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "SyncId" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = icalproperty_get_value_as_string( prop );
        SetSyncId( tmpstr );
    } else
        SetSyncId( "" );

    //allday
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "AllDay" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_value_as_string( prop );
        if( strcmp( tmpstr, "TRUE" ) == 0 )
            m_allday= true;
        else
            m_allday= false;
    } else
        m_allday= false;

    //alarm
    icalcomponent *valarm = icalcomponent_get_first_component( vevent, ICAL_VALARM_COMPONENT );
    
    if ( valarm != 0) {
        m_hasalarm= true;
        prop = icalcomponent_get_first_property( valarm, ICAL_TRIGGER_PROPERTY );
        if( prop ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_RELATED_PARAMETER );
            if( tmppar ) {
                m_alarmtriggerrelation = icalparameter_get_related( tmppar );
            } else 
                m_alarmtriggerrelation = ICAL_RELATED_START;
        }
    }
    else
        m_hasalarm= false;

    //alarmunits
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "AlarmUnits" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = icalproperty_get_value_as_string( prop );
        SetAlarmUnits( tmpstr );
    } else
        SetAlarmUnits( DEFAULT_ALARM_UNITS );
    //alarmlength
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = (char *)icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "AlarmLength" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_value_as_string( prop );
        m_alarmlength= atol( tmpstr );
    } else
        m_alarmlength = DEFAULT_ALARM_LENGTH;

    //alarmemail
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = (char *)icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "AlarmEmailAddress" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = icalproperty_get_value_as_string( prop );
        SetAlarmEmailAddress( tmpstr );
    } else if( m_alarmemail ) {
        nsMemory::Free( m_alarmemail );
        m_alarmemail= nsnull;
    }

    //inviteemail
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "InviteEmailAddress" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_value_as_string( prop );
        SetInviteEmailAddress( tmpstr );
    } else if( m_inviteemail ) {
        nsMemory::Free( m_inviteemail );
        m_inviteemail= nsnull;
    }

    //recurinterval
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "RecurInterval" ) == 0 )
                break;
        }
    }
    
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_value_as_string( prop );
        m_recurinterval= atol( tmpstr );
    } else
        m_recurinterval= 0;

    //recurunits
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "RecurUnits" ) == 0 )
                break;
        }
    }

    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_value_as_string( prop );
        SetRecurUnits( tmpstr );
    } else
        SetRecurUnits( DEFAULT_RECUR_UNITS );

    //startdate
    prop = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if ( prop != 0) {
        m_start->m_datetime = icalproperty_get_dtstart( prop );
        bool datevalue=m_start->m_datetime.is_date;
        m_start->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
        const char *tzid=nsnull;
        if( m_start->m_datetime.is_utc && !datevalue )
            tzid="/Mozilla.org/BasicTimezones/GMT";
        m_start->m_datetime.is_utc = false;
        if( datevalue ) {
            m_allday = true;
            m_start->SetHour( 0 );
            m_start->SetMinute( 0 );
            m_start->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
        }
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_TZID_PARAMETER );
        if( tmppar )
            tzid = icalparameter_get_tzid( tmppar );
        if( tzid ) {
            if( !datevalue ) {
                PRTime timeinms;
                m_start->GetTime( &timeinms );
                m_start->SetTimeInTimezone( timeinms, tzid );
            } else {
                m_start->SetTzID( tzid );
            }
        }
    } else {
        m_start->m_datetime = icaltime_null_time();
    }
    
    //duration
    prop = icalcomponent_get_first_property( vevent, ICAL_DURATION_PROPERTY );
    if ( prop != 0) {
        m_duration = icalproperty_get_duration( prop );
    } else {
        m_duration = icaldurationtype_null_duration();
    }

    //enddate
    if( icaldurationtype_is_null_duration( m_duration ) ) {
        prop = icalcomponent_get_first_property( vevent, ICAL_DTEND_PROPERTY );
        if ( prop != 0) {
            m_end->m_datetime = icalproperty_get_dtend( prop );
            bool datevalue=m_end->m_datetime.is_date;
            m_end->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
            const char *tzid=nsnull;
            if( m_end->m_datetime.is_utc  && !datevalue )
                tzid="/Mozilla.org/BasicTimezones/GMT";
            m_end->m_datetime.is_utc = false;
            if( datevalue ) {
                m_end->SetHour( 0 );
                m_end->SetMinute( 0 );
            }
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_TZID_PARAMETER );
            if( tmppar )
                tzid = icalparameter_get_tzid( tmppar );
            if( tzid ) {
                if( !datevalue ) {
                    PRTime timeinms;
                    m_end->GetTime( &timeinms );
                    m_end->SetTimeInTimezone( timeinms, tzid );
                } else {
                    m_end->SetTzID( tzid );
                }
            }
        } else {
            prop = icalcomponent_get_first_property( vevent, ICAL_DUE_PROPERTY );
            if ( prop != 0) {
                m_end->m_datetime = icalproperty_get_due( prop );
                bool datevalue=m_end->m_datetime.is_date;
                m_end->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
                const char *tzid=nsnull;
                if( m_end->m_datetime.is_utc  && !datevalue )
                    tzid="/Mozilla.org/BasicTimezones/GMT";
                m_end->m_datetime.is_utc = false;
                if( datevalue ) {
                    m_end->SetHour( 0 );
                    m_end->SetMinute( 0 );
                }
                icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_TZID_PARAMETER );
                if( tmppar )
                    tzid = icalparameter_get_tzid( tmppar );
                if( tzid ) {
                    if( !datevalue ) {
                        PRTime timeinms;
                        m_end->GetTime( &timeinms );
                        m_end->SetTimeInTimezone( timeinms, tzid );
                    } else {
                        m_end->SetTzID( tzid );
                    }
                }
            } else {
                if( m_type == ICAL_VEVENT_COMPONENT && !icaltime_is_null_time( m_start->m_datetime ) ) {
                    m_end->m_datetime = m_start->m_datetime;
                } else {
                    m_end->m_datetime = icaltime_null_time();
                }
            }
        }
    } else {
        if( !icaltime_is_null_time( m_start->m_datetime ) ) {
            m_end->m_datetime = icaltime_add( m_start->m_datetime, m_duration );
        } else {
            m_end->m_datetime = icaltime_null_time();
        }
    }
    
    //stampdate
    prop = icalcomponent_get_first_property( vevent, ICAL_DTSTAMP_PROPERTY );
    if ( prop != 0) {
        m_stamp->m_datetime = icalproperty_get_dtstamp( prop );
    }

    //lastmodifed
    prop = icalcomponent_get_first_property( vevent, ICAL_LASTMODIFIED_PROPERTY );
    if ( prop != 0) {
        m_lastmodified = icalproperty_get_dtstamp( prop );
    } else {
        m_lastmodified = icaltime_null_time();
    }

    // scan for X- properties

    // first set the defaults
    m_lastalarmack = icaltime_null_time();

    const char *propvalue;
    const char *x_name;

    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {

        x_name = icalproperty_get_x_name ( prop );
        if( !x_name )
            continue;

        propvalue = icalproperty_get_value_as_string( prop );
        //lastalarmack
        if ( strcmp( x_name, XPROP_LASTALARMACK ) == 0) {
            m_lastalarmack = icaltime_from_string( propvalue );
        }
        //syncid
        else if ( strcmp( x_name, XPROP_SYNCID ) == 0) {
            SetSyncId( propvalue );
        }
        //alarmunits
        else if ( strcmp( x_name, XPROP_ALARMUNITS ) == 0) {
            SetAlarmUnits( propvalue );
        }
        //alarmlength
        else if ( strcmp( x_name, XPROP_ALARMLENGTH ) == 0) {
            m_alarmlength= atol( propvalue );
        }
        //recurunits
        else if ( strcmp( x_name, XPROP_RECURUNITS ) == 0) {
            SetRecurUnits( propvalue );
        }
        //recurinterval
        else if ( strcmp( x_name, XPROP_RECURINTERVAL ) == 0) {
            m_recurinterval= atol( propvalue );
        }
    }

    //recurend & recurforever & recur & recurweekday & recurweeknumber
    m_recur = false;
    m_recurforever = true;
    m_recurweekdays = 0;
    m_recurweeknumber = 0;
    prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        m_recur = true;
        struct icalrecurrencetype recur;
        recur = icalproperty_get_rrule(prop);
        m_recurend->m_datetime = recur.until;
        m_recurend->m_datetime.is_date = false;
        m_recurend->m_datetime.is_utc = false;
	    m_recurinterval = recur.interval;
	    m_recurcount = recur.count;
        if( !icaltime_is_null_time( recur.until ) )
            m_recurforever = false;
        if( recur.freq == ICAL_DAILY_RECURRENCE ) {
	        SetRecurUnits( "days" );
	    } else if( recur.freq == ICAL_WEEKLY_RECURRENCE ) {
            int k=0;
	        SetRecurUnits( "weeks" );
            while( recur.by_day[k] != ICAL_RECURRENCE_ARRAY_MAX ) {
                m_recurweekdays += 1 << (recur.by_day[k]-1);
                k++;
            }
        } else if( recur.freq == ICAL_MONTHLY_RECURRENCE ) {
            SetRecurUnits( "months" );
            if( recur.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX )
                m_recurweeknumber = icalrecurrencetype_day_position(recur.by_day[0]);
            if( m_recurweeknumber < 0 )
                m_recurweeknumber = 5;
        }
        else if( recur.freq == ICAL_YEARLY_RECURRENCE ) {
	        SetRecurUnits( "years" );
        }
    }
    
    //recur exceptions
    RemoveAllExceptions();
    for( prop = icalcomponent_get_first_property( vevent, ICAL_EXDATE_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_EXDATE_PROPERTY ) ) {
        icaltimetype exdate = icalproperty_get_exdate( prop );
        PRTime *exdateinms = new PRTime;
        if (!exdateinms)
          return NS_ERROR_OUT_OF_MEMORY;
        *exdateinms = ConvertToPrtime( exdate );
        m_exceptiondates.AppendElement( exdateinms );
    }

    for( int i=0; i<m_snoozetimes.Count(); i++ ) {
        PRTime *snoozetimeptr = (PRTime*)(m_snoozetimes[i]);
        delete snoozetimeptr;
    }
    m_snoozetimes.Clear();
    //snoozetimes
    icalcomponent *tmpcomp = icalcomponent_get_first_component( vevent, ICAL_X_COMPONENT );
    if ( tmpcomp != 0) {
        for( prop = icalcomponent_get_first_property( tmpcomp, ICAL_DTSTAMP_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( tmpcomp, ICAL_DTSTAMP_PROPERTY ) ) {
            icaltimetype snoozetime = icalproperty_get_dtstamp( prop );
            if( !icaltime_is_null_time( m_lastalarmack ) && icaltime_compare( m_lastalarmack, snoozetime ) >= 0 )
                continue;
            PRTime *snoozetimeinms = new PRTime;
            if (!snoozetimeinms)
              return NS_ERROR_OUT_OF_MEMORY;
            *snoozetimeinms = icaltime_as_timet( snoozetime );
            *snoozetimeinms = *snoozetimeinms * 1000;
            m_snoozetimes.AppendElement( snoozetimeinms );
        }
    }

#ifdef MOZ_MAIL_NEWS
    //attachments
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
        icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
        if ( tmppar != 0 ) {
            tmpstr = icalparameter_get_member( tmppar );
            if( strcmp( tmpstr, "Attachment" ) == 0 ) {
                nsresult rv;
                tmpstr = (char *)icalproperty_get_value_as_string( prop );
                nsCOMPtr<nsIMsgAttachment> attachment = do_CreateInstance( "@mozilla.org/messengercompose/attachment;1", &rv);
                if ( NS_SUCCEEDED(rv) && attachment ) {
                    attachment->SetUrl( tmpstr );
                    AddAttachment( attachment );
                }
            }
        }
    }
#endif

#ifdef MOZ_MAIL_NEWS
    //contacts
    for(prop = icalcomponent_get_first_property( vevent, ICAL_CONTACT_PROPERTY );
        prop != 0 ;
        prop = icalcomponent_get_next_property( vevent, ICAL_CONTACT_PROPERTY ) ) {
        tmpstr = icalproperty_get_contact( prop );
        nsresult rv;
        nsCOMPtr<nsIAbCard> contact = do_CreateInstance( "@mozilla.org/addressbook/cardproperty;1", &rv);
        if ( NS_SUCCEEDED(rv) && contact ) {
            nsAutoString email;
            email.AssignWithConversion( tmpstr );
            contact->SetPrimaryEmail( email.get() );
            AddContact( contact );
        }
    }
#endif

    return true;
}

icalcomponent* oeICalEventImpl::AsIcalComponent()
{
#ifdef ICAL_DEBUG_ALL
    printf( "AsIcalComponent()\n" );
#endif
    icalcomponent *newcalendar;
    
    newcalendar = icalcomponent_new_vcalendar();
    if ( !newcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::AsIcalComponent() failed: Cannot create VCALENDAR!\n" );
        #endif
        return nsnull;
    }

    //version
    icalproperty *prop = icalproperty_new_version( ICALEVENT_VERSION );
    icalcomponent_add_property( newcalendar, prop );

    //prodid
    prop = icalproperty_new_prodid( ICALEVENT_PRODID );
    icalcomponent_add_property( newcalendar, prop );

    //method
    if( m_method != 0 ){
        prop = icalproperty_new_method( (icalproperty_method) m_method );
        icalcomponent_add_property( newcalendar, prop );
    }

    icalcomponent *vevent = icalcomponent_new_vevent();

    //id
    prop = icalproperty_new_uid( m_id );
    icalcomponent_add_property( vevent, prop );

    //title
    if( !m_title.IsEmpty() ) {
        prop = icalproperty_new_summary( PromiseFlatCString( m_title ).get() );
        icalcomponent_add_property( vevent, prop );
    }
    //description
    if( !m_description.IsEmpty() ) {
        prop = icalproperty_new_description( PromiseFlatCString( m_description ).get() );
        icalcomponent_add_property( vevent, prop );
    }

    //location
    if( !m_location.IsEmpty() ) {
        prop = icalproperty_new_location( PromiseFlatCString( m_location ).get() );
        icalcomponent_add_property( vevent, prop );
    }

    //category
    if( !m_category.IsEmpty() ) {
        prop = icalproperty_new_categories( PromiseFlatCString( m_category ).get() );
        icalcomponent_add_property( vevent, prop );
    }

    //url
    if( !m_url.IsEmpty() ) {
        prop = icalproperty_new_url( PromiseFlatCString( m_url ).get() );
        icalcomponent_add_property( vevent, prop );
    }

    //priority
    if( m_priority != 0) {
        prop = icalproperty_new_priority( m_priority );
                    icalcomponent_add_property( vevent, prop );
    }

    //status
    if( m_status != 0 ){
        prop = icalproperty_new_status( (icalproperty_status) m_status );
        icalcomponent_add_property( vevent, prop );
    }

    //isprivate
    if( m_isprivate )
        prop = icalproperty_new_class( ICAL_CLASS_PRIVATE );
    else
        prop = icalproperty_new_class( ICAL_CLASS_PUBLIC );
    icalcomponent_add_property( vevent, prop );

    //syncId
    if( m_syncid && strlen( m_syncid ) != 0 ){
        prop = icalproperty_new_x( m_syncid );
        icalproperty_set_x_name( prop, XPROP_SYNCID);
        icalcomponent_add_property( vevent, prop );
    }

    //alarm
    if( m_hasalarm ) {
        struct icaltriggertype trig;
        icalcomponent *valarm = icalcomponent_new_valarm();
        trig.time.year = trig.time.month = trig.time.day = trig.time.hour = trig.time.minute = trig.time.second = 0;
        trig.duration.is_neg = true;
        trig.duration.days = trig.duration.weeks = trig.duration.hours = trig.duration.minutes = trig.duration.seconds = 0;
        if( m_alarmunits ) {
            if( strcasecmp( m_alarmunits, "days" ) == 0 )
                trig.duration.days = m_alarmlength;
            else if( strcasecmp( m_alarmunits, "hours" ) == 0 )
                trig.duration.hours = m_alarmlength;
            else
                trig.duration.minutes = m_alarmlength;
        } else
            trig.duration.minutes = m_alarmlength;

        if( m_alarmlength == 0 )
            trig.duration.seconds = 1;

        prop = icalproperty_new_trigger( trig );

        if( m_alarmtriggerrelation != DEFAULT_ALARMTRIGGER_RELATION ) {
            icalparameter *tmppar = icalparameter_new_related( m_alarmtriggerrelation );
            icalproperty_add_parameter( prop, tmppar );
        }

        icalcomponent_add_property( valarm, prop );
        icalcomponent_add_component( vevent, valarm );
    }

    //alarmunits
    if( m_alarmunits && strlen( m_alarmunits ) != 0 && strcasecmp( m_alarmunits, DEFAULT_ALARM_UNITS ) != 0 ){
        prop = icalproperty_new_x( m_alarmunits );
        icalproperty_set_x_name( prop, XPROP_ALARMUNITS);
        icalcomponent_add_property( vevent, prop );
    }

    char tmpstr[20];
    //alarmlength
    if( m_alarmlength != DEFAULT_ALARM_LENGTH ) {
        sprintf( tmpstr, "%lu", m_alarmlength );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_set_x_name( prop, XPROP_ALARMLENGTH);
        icalcomponent_add_property( vevent, prop );
    }

    //alarmemail
    if( m_alarmemail && strlen( m_alarmemail ) != 0 ){
        icalparameter *tmppar = icalparameter_new_member( "AlarmEmailAddress" );
        prop = icalproperty_new_x( m_alarmemail );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    //lastalarmack
    if( !icaltime_is_null_time( m_lastalarmack ) ) {
        prop = icalproperty_new_x( icaltime_as_ical_string(m_lastalarmack ));
        icalproperty_set_x_name( prop, XPROP_LASTALARMACK);
	icalcomponent_add_property( vevent, prop );
    }

    //inviteemail
    if( m_inviteemail && strlen( m_inviteemail ) != 0 ){
        icalparameter *tmppar = icalparameter_new_member( "InviteEmailAddress" );
        prop = icalproperty_new_x( m_inviteemail );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }

    //Create enddate if does not exist
    if( icaltime_is_null_time( m_end->m_datetime ) && !icaltime_is_null_time( m_start->m_datetime ) ) {
        //Set to the same as start date
        m_end->m_datetime = m_start->m_datetime;
    }

    //recurunits
    if( m_recurunits && strlen( m_recurunits ) != 0 && strcasecmp( m_recurunits, DEFAULT_RECUR_UNITS ) != 0 ){
        prop = icalproperty_new_x( m_recurunits );
        icalproperty_set_x_name( prop, XPROP_RECURUNITS);
        icalcomponent_add_property( vevent, prop );
    }

    //recurinterval
    if( m_recurinterval != 1 ){
        sprintf( tmpstr, "%lu", m_recurinterval );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_set_x_name( prop, XPROP_RECURINTERVAL);
        icalcomponent_add_property( vevent, prop );
    }

    //recurrence

    if( m_recur ) {
        int recurtype = RECUR_NONE;
        int interval = m_recurinterval;
        if( interval == 0 )
            interval = 1;
        if( m_recurunits ) {
            if( strcasecmp( m_recurunits , "days" ) == 0 ) {
               recurtype = RECUR_DAILY;
            } else if( strcasecmp( m_recurunits , "weeks" ) == 0 ) {
               recurtype = RECUR_WEEKLY;
            } else if( strcasecmp( m_recurunits , "months" ) == 0 ) {
                recurtype = RECUR_MONTHLY_MDAY;
            } else if( strcasecmp( m_recurunits, "months_day" ) == 0 ) {
                recurtype = RECUR_MONTHLY_WDAY;
            } else if( strcasecmp( m_recurunits , "years" ) == 0 ) {
                recurtype = RECUR_YEARLY;
            }
        }
        
        struct icalrecurrencetype recur;
        icalrecurrencetype_clear( &recur );
        //    for( int i=0; i<ICAL_BY_MONTH_SIZE; i++ )
        //        recur.by_month[i] = i;
        recur.interval = interval;
        recur.until.is_utc = false;
        recur.until.is_date = true;
        if( m_recurforever && m_recurcount == 0 ) {
            recur.until.year = 0;
            recur.until.month = 0;
            recur.until.day = 0;
            recur.until.hour = 0;
            recur.until.minute = 0;
            recur.until.second = 0;
        } else {
            if( m_recurcount == 0 ) {
                recur.until.year = m_recurend->m_datetime.year;
                recur.until.month = m_recurend->m_datetime.month;
                recur.until.day = m_recurend->m_datetime.day;
                recur.until.hour = 23;
                recur.until.minute = 59;
                recur.until.second = 59;
            } else {
                recur.count = m_recurcount;
            }
        }

        switch ( recurtype ) {
            case RECUR_NONE:
                break;
            case RECUR_DAILY:
                recur.freq = ICAL_DAILY_RECURRENCE;
                prop = icalproperty_new_rrule( recur );
                icalcomponent_add_property( vevent, prop );
                break;
            case RECUR_WEEKLY:{
                recur.freq = ICAL_WEEKLY_RECURRENCE;
                //if the recur rule is weekly make sure a weekdaymask exists
                if( m_recurweekdays == 0 ) {
                    m_recurweekdays = 1 << (icaltime_day_of_week( m_start->m_datetime )-1);
                }
                short weekdaymask = m_recurweekdays;
                int k=0;
//                bool weekdaymatchesstartdate=false;
                for( int i=0; i<7; i++ ) {
                    if( weekdaymask & 1 ) {
                        recur.by_day[k++]=i+1;
//                        if( !weekdaymatchesstartdate ) {
//                            m_start->AdjustToWeekday( i+1 );
//                            m_end->AdjustToWeekday( i+1 );
//                            weekdaymatchesstartdate=true;
//                        }
                    }
                    weekdaymask >>= 1;
                }
                prop = icalproperty_new_rrule( recur );
                icalcomponent_add_property( vevent, prop );
                break;
            }
            case RECUR_MONTHLY_MDAY:
                recur.freq = ICAL_MONTHLY_RECURRENCE;
                if( m_recurweeknumber ) {
//                    printf( "DAY: %d\n" , icaltime_day_of_week( m_start->m_datetime ) );
//                    printf( "WEEKNUMBER: %d\n" , m_recurweeknumber );
                    if( m_recurweeknumber != 5 )
                        recur.by_day[0] = icaltime_day_of_week( m_start->m_datetime ) + m_recurweeknumber*8;
                    else
                        recur.by_day[0] = - icaltime_day_of_week( m_start->m_datetime ) - 8 ;
                }
                prop = icalproperty_new_rrule( recur );
                icalcomponent_add_property( vevent, prop );
                break;
            case RECUR_MONTHLY_WDAY:
                recur.freq = ICAL_MONTHLY_RECURRENCE;
                prop = icalproperty_new_rrule( recur );
                icalcomponent_add_property( vevent, prop );
                break;
            case RECUR_YEARLY:
                recur.by_month[0] = m_start->m_datetime.month;
                recur.freq = ICAL_YEARLY_RECURRENCE;
                prop = icalproperty_new_rrule( recur );
                icalcomponent_add_property( vevent, prop );
                break;
        }

        //exceptions
        for( int i=0; i<m_exceptiondates.Count(); i++ ) {
            icaltimetype exdate = ConvertFromPrtime( *(PRTime *)(m_exceptiondates[i]) );
            prop = icalproperty_new_exdate( exdate );
            icalcomponent_add_property( vevent, prop );
        }
    }

    //startdate
    char *starttzid=nsnull;
    if( m_start && !icaltime_is_null_time( m_start->m_datetime ) ) {
        m_start->GetTzID( &starttzid );
        if( !starttzid && m_storeingmt ) {
            m_start->SetTzID( "/Mozilla.org/BasicTimezones/GMT" );
            m_start->GetTzID( &starttzid );
        }

        if( m_allday ) {
            m_start->SetHour( 0 );
            m_start->SetMinute( 0 );
            m_start->m_datetime.is_date = true; //This will reflect the event being an all-day event
            prop = icalproperty_new_dtstart( m_start->m_datetime );
            if( starttzid ) {
                icaltimezone *timezone = icaltimezone_get_builtin_timezone_from_tzid (starttzid);
                icalparameter *tmppar = icalparameter_new_tzid( starttzid );
                icalproperty_add_parameter( prop, tmppar );
                icalcomponent_add_component( newcalendar, icalcomponent_new_clone( icaltimezone_get_component ( timezone ) ) );
            }
            m_start->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
        } else {
            if( starttzid ) {
                icaltimezone *timezone = icaltimezone_get_builtin_timezone_from_tzid (starttzid);
                icaltimetype convertedtime = m_start->m_datetime;
                icaltimezone_convert_time ( &convertedtime, currenttimezone, timezone );
                if( strcmp( starttzid, "/Mozilla.org/BasicTimezones/GMT" )==0 ) {
                    convertedtime.is_utc = true;
                    prop = icalproperty_new_dtstart( convertedtime );
                } else {
                    prop = icalproperty_new_dtstart( convertedtime );
                    icalparameter *tmppar = icalparameter_new_tzid( starttzid );
                    icalproperty_add_parameter( prop, tmppar );
                    icalcomponent_add_component( newcalendar, icalcomponent_new_clone( icaltimezone_get_component ( timezone ) ) );
                }
            } else 
                prop = icalproperty_new_dtstart( m_start->m_datetime );
        }
        icalcomponent_add_property( vevent, prop );
    }

    //enddate
    if( m_end && !icaltime_is_null_time( m_end->m_datetime ) ) {
        char *tzid=nsnull;
        m_end->GetTzID( &tzid );
        if( !tzid && m_storeingmt ) {
            m_end->SetTzID( "/Mozilla.org/BasicTimezones/GMT" );
            m_end->GetTzID( &tzid );
        }
        if( m_allday ) {
            if( m_end->CompareDate( m_start )==0 ) {
                m_end->m_datetime = m_start->m_datetime;
                icaltime_adjust( &(m_end->m_datetime), 1, 0, 0, 0 );
            } else {
                m_end->SetHour( 0 );
                m_end->SetMinute( 0 );
            }
            m_end->m_datetime.is_date = true;
            prop = icalproperty_new_dtend( m_end->m_datetime );
            if( tzid ) {
                icaltimezone *timezone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
                icalparameter *tmppar = icalparameter_new_tzid( tzid );
                icalproperty_add_parameter( prop, tmppar );
                if( !starttzid || strcmp( starttzid, tzid ) != 0 ) 
                    icalcomponent_add_component( newcalendar, icalcomponent_new_clone( icaltimezone_get_component ( timezone ) ) );
                nsMemory::Free( tzid );
            }
            m_end->m_datetime.is_date = false; //Because currently we depend on m_datetime being a complete datetime value.
        } else {
            if( tzid ) {
                icaltimezone *timezone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
                icaltimetype convertedtime = m_end->m_datetime;
                icaltimezone_convert_time ( &convertedtime, currenttimezone, timezone );
                if( strcmp( tzid, "/Mozilla.org/BasicTimezones/GMT" )==0 ) {
                    convertedtime.is_utc = true;
                    prop = icalproperty_new_dtend( convertedtime );
                } else {
                    prop = icalproperty_new_dtend( convertedtime );
                    icalparameter *tmppar = icalparameter_new_tzid( tzid );
                    icalproperty_add_parameter( prop, tmppar );
                    if( !starttzid || strcmp( starttzid, tzid ) != 0 ) 
                        icalcomponent_add_component( newcalendar, icalcomponent_new_clone( icaltimezone_get_component ( timezone ) ) );
                }
                nsMemory::Free( tzid );
            } else 
                prop = icalproperty_new_dtend( m_end->m_datetime );
        }
        icalcomponent_add_property( vevent, prop );
    }

    if( starttzid )
        nsMemory::Free( starttzid );

    //stampdate
    if( m_stamp && !icaltime_is_null_time( m_stamp->m_datetime ) ) {
        prop = icalproperty_new_dtstamp( m_stamp->m_datetime );
        icalcomponent_add_property( vevent, prop );
    }

    //lastmodified
    if( !icaltime_is_null_time( m_lastmodified ) ) {
        prop = icalproperty_new_lastmodified( m_lastmodified );
        icalcomponent_add_property( vevent, prop );
    }

    //snoozetimes
    icalcomponent *tmpcomp=NULL;
    int j;
    for( j=0; j<m_snoozetimes.Count(); j++ ) {
        if( tmpcomp == NULL )
            tmpcomp = icalcomponent_new( ICAL_X_COMPONENT );
        icaltimetype snoozetime = ConvertFromPrtime( *(PRTime *)(m_snoozetimes[j]) );
        prop = icalproperty_new_dtstamp( snoozetime );
        icalcomponent_add_property( tmpcomp, prop );
    }
    if( tmpcomp )
        icalcomponent_add_component( vevent, tmpcomp );

#ifdef MOZ_MAIL_NEWS
    PRUint32 attachmentCount = 0;
    m_attachments->Count(&attachmentCount);
    nsCOMPtr<nsIMsgAttachment> element;
    unsigned int i;
    for (i = 0; i < attachmentCount; i ++) {
        m_attachments->QueryElementAt(i, NS_GET_IID(nsIMsgAttachment), getter_AddRefs(element));
        if (element)
        {
            char *url;
            element->GetUrl( &url );
            icalparameter *tmppar = icalparameter_new_member( "Attachment" );
            prop = icalproperty_new_x( url );
            icalproperty_add_parameter( prop, tmppar );
            icalcomponent_add_property( vevent, prop );
/*            icalattach *attach= icalattach_new_from_url( url );
            if( attach ) {
                char tst[100]= "testing";
                char *buffer;
                buffer = PL_Base64Encode( tst, strlen(tst), nsnull );

//                strcpy( buffer, "salam" );
//                icalattachtype_set_base64( attachtype, buffer, 0 );
                prop = icalproperty_new_attach( attach );

//                tmppar = icalparameter_new_fmttype( url );
//                icalproperty_add_parameter( prop, tmppar );
//                tmppar = icalparameter_new_encoding( ICAL_ENCODING_BASE64 );
//                icalproperty_add_parameter( prop, tmppar );

                icalcomponent_add_property( vevent, prop );
            }*/
            nsMemory::Free( url );
        }
    }

    PRUint32 contactCount = 0;
    m_contacts->Count(&contactCount);
    nsCOMPtr<nsIAbCard> contact;
    for (i = 0; i < contactCount; i ++) {
        m_contacts->QueryElementAt(i, NS_GET_IID(nsIAbCard), getter_AddRefs(contact));
        if (contact)
        {
            nsXPIDLString email;
            contact->GetPrimaryEmail( getter_Copies( email ) );
            NS_ConvertUCS2toUTF8 aUtf8Str(email);
            prop = icalproperty_new_contact( aUtf8Str.get() );
            icalcomponent_add_property( vevent, prop );
        }
    }
#endif

    //add event to newcalendar
    icalcomponent_add_component( newcalendar, vevent );
    return newcalendar;
}

NS_IMETHODIMP oeICalEventImpl::ReportError( PRInt16 severity, PRUint32 errorid, const char *errorstring ) {
    if( m_calendar ) {
        m_calendar->ReportError( severity, errorid, errorstring );
    } else if( gContainer ) {
        gContainer->ReportError( severity, errorid, errorstring );
    }
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetParameter( const char *name, const char *value ) {
    if( strcmp( name, "ICAL_RELATED_PARAMETER" ) == 0 ) {
        if( strcmp( value, "ICAL_RELATED_START" ) == 0 ) 
            m_alarmtriggerrelation = ICAL_RELATED_START;
        else if( strcmp( value, "ICAL_RELATED_END" ) == 0 )
            m_alarmtriggerrelation = ICAL_RELATED_END;
        else
            return NS_ERROR_ILLEGAL_VALUE;

        return NS_OK;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalEventImpl::GetParameter( const char *name, char **value ) {
    *value = nsnull;
    if( strcmp( name, "ICAL_RELATED_PARAMETER" ) == 0 ) {
        char *tmpstr=nsnull;
        switch ( m_alarmtriggerrelation ) {
            case ICAL_RELATED_START:
                tmpstr = "ICAL_RELATED_START";
                break;
            case ICAL_RELATED_END:
                tmpstr = "ICAL_RELATED_END";
                break;
        }
        if( tmpstr ) {
            *value = (char*) nsMemory::Clone( tmpstr, strlen( tmpstr )+1);
            return NS_OK;
        }
        else
            return NS_ERROR_UNEXPECTED;
    }
    return NS_ERROR_NOT_AVAILABLE;
}

/********************************************************************************************/
#include "nsIServiceManager.h"
 
NS_IMPL_ADDREF(oeICalEventDisplayImpl)
NS_IMPL_RELEASE(oeICalEventDisplayImpl)
//NS_IMPL_ISUPPORTS1(oeICalEventDisplayImpl, oeIICalEventDisplay)

nsresult
NS_NewICalEventDisplay( oeIICalEvent* event, oeIICalEventDisplay** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeICalEventDisplayImpl( event );
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeICalEventDisplayImpl::oeICalEventDisplayImpl( oeIICalEvent* event )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::oeICalEventDisplayImpl(): %d\n", ++gEventDisplayCount );
#endif
    nsresult rv;
    if( event == nsnull ) {
        mEvent = do_CreateInstance(OE_ICALEVENTDISPLAY_CONTRACTID, &rv);
    } else {
        mEvent = event;
    }

    /* member initializers and constructor code */
    m_displaydate = icaltime_null_time();
}

oeICalEventDisplayImpl::~oeICalEventDisplayImpl()
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::~oeICalEventDisplayImpl(): %d\n", --gEventDisplayCount );
#endif
  /* destructor code */
    mEvent = nsnull;
}

NS_IMETHODIMP
oeICalEventDisplayImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (nsnull == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = (void*)(oeIICalEventDisplay*)this;
        *aInstancePtr = (nsISupports*)*aInstancePtr;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(oeIICalEventDisplay))) {
        *aInstancePtr = (void*)(oeIICalEventDisplay*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(oeIICalEvent))) {
        return mEvent->QueryInterface( aIID, aInstancePtr );
    }
    if (aIID.Equals(NS_GET_IID(oeIICalTodo))) {
        return mEvent->QueryInterface( aIID, aInstancePtr );
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP oeICalEventDisplayImpl::GetDisplayDate( PRTime *aRetVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::GetDisplayDate()\n" );
#endif
    *aRetVal = ConvertToPrtime( m_displaydate );
    return NS_OK;
}
    

NS_IMETHODIMP oeICalEventDisplayImpl::SetDisplayDate( PRTime aNewVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::SetDisplayDate()\n" );
#endif
    m_displaydate = ConvertFromPrtime( aNewVal );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventDisplayImpl::GetDisplayEndDate( PRTime *aRetVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::GetDisplayEndDate()\n" );
#endif
    *aRetVal = ConvertToPrtime( m_displaydateend );
    return NS_OK;
}
    

NS_IMETHODIMP oeICalEventDisplayImpl::SetDisplayEndDate( PRTime aNewVal )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::SetDisplayEndDate()\n" );
#endif
    m_displaydateend = ConvertFromPrtime( aNewVal );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventDisplayImpl::GetEvent( oeIICalEvent **ev )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::GetEvent()\n" );
#endif
#ifdef ICAL_DEBUG_ALL
    printf( "WARNING: .event is no longer needed to access event fields\n" );
#endif
    *ev = mEvent;
    NS_ADDREF(*ev);
    return NS_OK;
}
