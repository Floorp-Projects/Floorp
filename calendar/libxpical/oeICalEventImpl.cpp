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

#ifndef WIN32
#include <unistd.h>
#endif

#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "plbase64.h"
#include "nsMsgCompCID.h"

#define strcasecmp strcmp

#define RECUR_NONE 0
#define RECUR_DAILY 1
#define RECUR_WEEKLY 2
#define RECUR_MONTHLY_MDAY 3
#define RECUR_MONTHLY_WDAY 4
#define RECUR_YEARLY 5

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

int gEventCount = 0;
int gEventDisplayCount = 0;

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
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
    nsresult rv;
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_start ))) {
        m_start = nsnull;
	}
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_end ))) {
        m_end = nsnull;
	}
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_recurend ))) {
        m_recurend = nsnull;
	}
    m_id = nsnull;
    m_title = nsnull;
    m_description = nsnull;
    m_location = nsnull;
    m_category = nsnull;
    m_url = nsnull;
    m_isprivate = true;
    m_syncid = nsnull;
    m_allday = false;
    m_hasalarm = false;
    m_alarmlength = 0;
    m_alarmemail = nsnull;
    m_inviteemail = nsnull;
    m_recurinterval = 1;
    m_recur = false;
    m_recurforever = true;
    m_alarmunits = nsnull;
    m_recurunits = nsnull;
    m_recurweekdays = 0;
    m_recurweeknumber = 0;
    m_lastalarmack = icaltime_null_time();
    SetAlarmUnits( "minutes" );
    SetRecurUnits( "weeks" );
    SetSyncId( "" );
    NS_NewISupportsArray(getter_AddRefs(m_attachments));
}

oeICalEventImpl::~oeICalEventImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::~oeICalEventImpl(): %d\n", --gEventCount );
#endif
  /* destructor code */
  if( m_id )
        nsMemory::Free( m_id );
  if( m_title )
      nsMemory::Free( m_title );
  if( m_description )
      nsMemory::Free( m_description );
  if( m_location )
      nsMemory::Free( m_location );
  if( m_category )
      nsMemory::Free( m_category );
  if( m_url )
      nsMemory::Free( m_url );
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
  if( m_recurend )
    m_recurend->Release();
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
NS_IMETHODIMP oeICalEventImpl::GetTitle(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetTitle() = " );
#endif
    
    if( m_title ) {
        *aRetVal= (char*) nsMemory::Clone( m_title, strlen(m_title)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetTitle(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetTitle( %s )\n", aNewVal );
#endif
    
    if( m_title )
        nsMemory::Free( m_title );
    
    if( aNewVal )
        m_title= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_title = nsnull;

    return NS_OK;
}

/* attribute string Description; */
NS_IMETHODIMP oeICalEventImpl::GetDescription(char * *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetDescription() = " );
#endif
    if( m_description ) {
        *aRetVal= (char*) nsMemory::Clone( m_description , strlen(m_description)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
    
#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetDescription(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetDescription( %s )\n", aNewVal );
#endif

    if( m_description )
        nsMemory::Free( m_description );
    
    if( aNewVal )
        m_description= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_description = nsnull;
    
    return NS_OK;
}

/* attribute string Location; */
NS_IMETHODIMP oeICalEventImpl::GetLocation(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetLocation() = " );
#endif

    if( m_location ) {
        *aRetVal= (char*) nsMemory::Clone( m_location , strlen(m_location)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
   return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetLocation(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetLocation( %s )\n", aNewVal );
#endif

    if( m_location )
        nsMemory::Free( m_location );
    
    if( aNewVal )
        m_location= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_location = nsnull;

    return NS_OK;
}

/* attribute string Category; */
NS_IMETHODIMP oeICalEventImpl::GetCategories(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetCategories() = " );
#endif
    
    if( m_category ) {
        *aRetVal= (char*) nsMemory::Clone( m_category , strlen(m_category)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetCategories(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetCategories( %s )\n", aNewVal );
#endif
    if( m_category )
        nsMemory::Free( m_category );
    
    if( aNewVal )
        m_category= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_category = nsnull;
    return NS_OK;
}

/* attribute string Url; */
NS_IMETHODIMP oeICalEventImpl::GetUrl(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetUrl() = " );
#endif
    
    if( m_url ) {
        *aRetVal= (char*) nsMemory::Clone( m_url , strlen(m_url)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetUrl(const char * aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetUrl( %s )\n", aNewVal );
#endif
    if( m_url )
        nsMemory::Free( m_url );
    
    if( aNewVal )
        m_url= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_url = nsnull;
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
    printf( "SetAlarmLength(%lu)\n", aNewVal );
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
    printf( "SetAlarmEmailAddres()\n" );
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

/* attribute boolean RecurInterval; */
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
    printf( "SetRecurUnits()\n" );
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
    printf( "GetNextRecurrence()\n" );
#endif
    //for non recurring events
    *isvalid = false;
    if( !m_recur ) {
        PRTime start;
        m_start->GetTime( &start );

        PRInt64 startinsec,begininsec,msecpersec;
        LL_I2L( msecpersec, PR_MSEC_PER_SEC );
        LL_DIV( startinsec, start, msecpersec );
        LL_DIV( begininsec, begin, msecpersec );

        if( LL_CMP( startinsec, > , begininsec ) ) {
            *retval = start;
            *isvalid = true;
        }
        return NS_OK;
    }

    //for recurring events
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::GetNextRecurrence() failed!\n" );
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
                icaltime_normalize( next );
            }
            PRTime nextinms = ConvertToPrtime( next );
            if( LL_CMP(nextinms, > ,begin) && !IsExcepted( nextinms ) ) {
//                printf( "Result: %d-%d-%d %d:%d\n" , next.year, next.month, next.day, next.hour, next.minute );
                *retval = nextinms;
                *isvalid = true;
                break;
            }
        }
        icalrecur_iterator_free(ritr);
    }

    icalcomponent_free( vcalendar );
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
                icaltime_normalize( next );
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

icaltimetype oeICalEventImpl::GetNextRecurrence( icaltimetype begin ) {
    icaltimetype result = icaltime_null_time();
    PRTime begininms = ConvertToPrtime( begin );
    PRTime resultinms;
    PRBool isvalid;
    GetNextRecurrence( begininms ,&resultinms, &isvalid );
    if( !isvalid )
        return result;

    result = ConvertFromPrtime( resultinms );
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

    icaltimetype starting = begin;

    if( !icaltime_is_null_time( m_lastalarmack ) && icaltime_compare( begin, m_lastalarmack ) < 0 )
        starting = m_lastalarmack;

    icaltimetype checkloop = starting;
    do {
        checkloop = GetNextRecurrence( checkloop );
        result = checkloop;
        if( icaltime_is_null_time( checkloop ) ) {
            break;
        }
        result = CalculateAlarmTime( result );
    } while ( icaltime_compare( starting, result ) >= 0 );

    for( unsigned int i=0; i<m_snoozetimes.size(); i++ ) {
        icaltimetype snoozetime = ConvertFromPrtime( m_snoozetimes[i] );
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

    return result;
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

NS_IMETHODIMP oeICalEventImpl::GetIcalString(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetIcalString() = " );
#endif
    
    *aRetVal = nsnull;
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::GetIcalString() failed!\n" );
        #endif
        return NS_OK;
    }

    char *str = icalcomponent_as_ical_string( vcalendar );
    if( str ) {
        *aRetVal= (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= EmptyReturn();
    icalcomponent_free( vcalendar );

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::ParseIcalString(const char *aNewVal, PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "ParseIcalString( %s )\n", aNewVal );
#endif
    
    *aRetVal = false;
    icalcomponent *comp = icalparser_parse_string( aNewVal );
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
    m_exceptiondates.push_back( exdate );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::RemoveAllExceptions()
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventImpl::RemoveAllExceptions()\n" );
#endif
    m_exceptiondates.clear();
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
        for( unsigned int i=0; i<m_exceptiondates.size(); i++ ) {
            current = m_exceptiondates[i];
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
    for( unsigned int i=0; i<m_exceptiondates.size(); i++ ) {
        if( m_exceptiondates[i] == date ) {
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
    snoozetime *= 1000;
    m_snoozetimes.push_back( snoozetime );
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
}

NS_IMETHODIMP oeICalEventImpl::RemoveAttachment(nsIMsgAttachment *attachment)
{
#ifdef ICAL_DEBUG
    printf( "oeICalEventImpl::RemoveAttachment()\n" );
#endif
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

    const char *tmpstr;
//id    
    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    if ( prop ) {
        tmpstr = icalproperty_get_uid( prop );
        SetId( tmpstr );
    } else {
        #ifdef ICAL_DEBUG
        printf( "oeICalEventImpl::ParseIcalComponent() failed: UID not found!\n" );
        #endif
        return false;
    }

//title
    prop = icalcomponent_get_first_property( vevent, ICAL_SUMMARY_PROPERTY );
    if ( prop != 0) {
        tmpstr = icalproperty_get_summary( prop );
        SetTitle( tmpstr );
    } else if( m_title ) {
        nsMemory::Free( m_title );
        m_title = nsnull;
    }

//description
    prop = icalcomponent_get_first_property( vevent, ICAL_DESCRIPTION_PROPERTY );
    if ( prop != 0) {
        tmpstr = icalproperty_get_description( prop );
        SetDescription( tmpstr );
    } else if( m_description ) {
        nsMemory::Free( m_description );
        m_description = nsnull;
    }

//location
    prop = icalcomponent_get_first_property( vevent, ICAL_LOCATION_PROPERTY );
    if ( prop != 0) {
        tmpstr = icalproperty_get_location( prop );
        SetLocation( tmpstr );
    } else if( m_location ) {
        nsMemory::Free( m_location );
        m_location = nsnull;
    }

//category
    prop = icalcomponent_get_first_property( vevent, ICAL_CATEGORIES_PROPERTY );
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_categories( prop );
        SetCategories( tmpstr );
    } else if( m_category ) {
        nsMemory::Free( m_category );
        m_category= nsnull;
    }

//url
    prop = icalcomponent_get_first_property( vevent, ICAL_URL_PROPERTY );
    if ( prop != 0) {
        tmpstr = (char *)icalproperty_get_url( prop );
        SetUrl( tmpstr );
    } else if( m_url ) {
        nsMemory::Free( m_url );
        m_url= nsnull;
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
    
    if ( valarm != 0)
        m_hasalarm= true;
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
        SetAlarmUnits( "minutes" );
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
        m_alarmlength = 0;

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

    //lastalarmack
    prop = icalcomponent_get_first_property( vevent, ICAL_DTSTAMP_PROPERTY );
    if ( prop != 0) {
        m_lastalarmack = icalproperty_get_dtstart( prop );
    } else {
        m_lastalarmack = icaltime_null_time();
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

//startdate
    prop = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if ( prop != 0) {
        m_start->m_datetime = icalproperty_get_dtstart( prop );
    } else {
        m_start->m_datetime = icaltime_null_time();
    }
//enddate
    prop = icalcomponent_get_first_property( vevent, ICAL_DTEND_PROPERTY );
    if ( prop != 0) {
        icaltimetype end;
        end = icalproperty_get_dtstart( prop );
        m_end->m_datetime = end;
    } else if( !icaltime_is_null_time( m_start->m_datetime ) ) {
        m_end->m_datetime = m_start->m_datetime;
    } else {
        m_end->m_datetime = icaltime_null_time();
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
        if( !icaltime_is_null_time( recur.until ) )
            m_recurforever = false;
        if( recur.freq == ICAL_WEEKLY_RECURRENCE ) {
            int k=0;
            while( recur.by_day[k] != ICAL_RECURRENCE_ARRAY_MAX ) {
                m_recurweekdays += 1 << (recur.by_day[k]-1);
                k++;
            }
        } else if( recur.freq == ICAL_MONTHLY_RECURRENCE ) {
            if( recur.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX )
                m_recurweeknumber = icalrecurrencetype_day_position(recur.by_day[0]);
            if( m_recurweeknumber < 0 )
                m_recurweeknumber = 5;
        }
    }
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
        SetRecurUnits( "weeks" );

    //recur exceptions
    m_exceptiondates.clear();
    for( prop = icalcomponent_get_first_property( vevent, ICAL_EXDATE_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_EXDATE_PROPERTY ) ) {
        icaltimetype exdate = icalproperty_get_exdate( prop );
        PRTime exdateinms = ConvertToPrtime( exdate );
        m_exceptiondates.push_back( exdateinms );
    }

    m_snoozetimes.clear();
    //snoozetimes
    icalcomponent *tmpcomp = icalcomponent_get_first_component( vevent, ICAL_X_COMPONENT );
    if ( tmpcomp != 0) {
        for( prop = icalcomponent_get_first_property( tmpcomp, ICAL_DTSTAMP_PROPERTY );
                prop != 0 ;
                prop = icalcomponent_get_next_property( tmpcomp, ICAL_DTSTAMP_PROPERTY ) ) {
            icaltimetype snoozetime = icalproperty_get_dtstamp( prop );
            if( !icaltime_is_null_time( m_lastalarmack ) && icaltime_compare( m_lastalarmack, snoozetime ) >= 0 )
                continue;
            PRTime snoozetimeinms = icaltime_as_timet( snoozetime );
            snoozetimeinms *= 1000;
            m_snoozetimes.push_back( snoozetimeinms );
        }
    }

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
                    nsCOMPtr<nsIMsgAttachment> attachment = do_CreateInstance(NS_MSGATTACHMENT_CONTRACTID, &rv);
                    if ( NS_SUCCEEDED(rv) && attachment ) {
                        attachment->SetUrl( tmpstr );
                        AddAttachment( attachment );
                    }
                }
            }
    }
    return true;
}

#define ICALEVENT_VERSION "2.0"
#define ICALEVENT_PRODID "PRODID:-//Mozilla.org/NONSGML Mozilla Calendar V1.0//EN"

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

    icalcomponent *vevent = icalcomponent_new_vevent();

    //id
    prop = icalproperty_new_uid( m_id );
    icalcomponent_add_property( vevent, prop );

    //title
    if( m_title && strlen( m_title ) != 0 ){
        prop = icalproperty_new_summary( m_title );
        icalcomponent_add_property( vevent, prop );
    }
    //description
    if( m_description && strlen( m_description ) != 0 ){
        prop = icalproperty_new_description( m_description );
        icalcomponent_add_property( vevent, prop );
    }

    //location
    if( m_location && strlen( m_location ) != 0 ){
        prop = icalproperty_new_location( m_location );
        icalcomponent_add_property( vevent, prop );
    }

    //category
    if( m_category && strlen( m_category ) != 0 ){
        prop = icalproperty_new_categories( m_category );
        icalcomponent_add_property( vevent, prop );
    }

    //url
    if( m_url && strlen( m_url ) != 0 ){
        prop = icalproperty_new_url( m_url );
        icalcomponent_add_property( vevent, prop );
    }

    //isprivate
    if( m_isprivate )
        prop = icalproperty_new_class( ICAL_CLASS_PRIVATE );
    else
        prop = icalproperty_new_class( ICAL_CLASS_PUBLIC );
    icalcomponent_add_property( vevent, prop );

    icalparameter *tmppar;
    
    //syncId
    if( m_syncid && strlen( m_syncid ) != 0 ){
        icalparameter *tmppar = icalparameter_new_member( "SyncId" );
        prop = icalproperty_new_x( m_syncid );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }

    //allday
    if( m_allday ) {
        tmppar = icalparameter_new_member( "AllDay" );
        prop = icalproperty_new_x( "TRUE" );
        icalproperty_add_parameter( prop, tmppar );
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
        icalcomponent_add_property( valarm, prop );
        icalcomponent_add_component( vevent, valarm );
    }

    //alarmunits
    if( m_alarmunits && strlen( m_alarmunits ) != 0 ){
        icalparameter *tmppar = icalparameter_new_member( "AlarmUnits" );
        prop = icalproperty_new_x( m_alarmunits );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }

    char tmpstr[20];
    //alarmlength
    if( m_alarmlength ) {
        sprintf( tmpstr, "%lu", m_alarmlength );
        tmppar = icalparameter_new_member( "AlarmLength" );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_add_parameter( prop, tmppar );
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
        prop = icalproperty_new_dtstamp( m_lastalarmack );
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
    if( icaltime_is_null_time( m_end->m_datetime ) ) {
        //Set to the same as start date 23:59
        m_end->m_datetime = m_start->m_datetime;
        m_end->SetHour( 23 ); m_end->SetMinute( 59 );
    }

    //recurunits
    if( m_recurunits && strlen( m_recurunits ) != 0 ){
        icalparameter *tmppar = icalparameter_new_member( "RecurUnits" );
        prop = icalproperty_new_x( m_recurunits );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }

    //recurinterval
    sprintf( tmpstr, "%lu", m_recurinterval );
    tmppar = icalparameter_new_member( "RecurInterval" );
    prop = icalproperty_new_x( tmpstr );
    icalproperty_add_parameter( prop, tmppar );
    icalcomponent_add_property( vevent, prop );

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
//               recurtype = RECUR_DAILY;
//               interval = 7 * m_recurinterval;
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
        if( m_recurforever ) {
            recur.until.year = 0;
            recur.until.month = 0;
            recur.until.day = 0;
            recur.until.hour = 0;
            recur.until.minute = 0;
            recur.until.second = 0;
        } else {
            recur.until.year = m_recurend->m_datetime.year;
            recur.until.month = m_recurend->m_datetime.month;
            recur.until.day = m_recurend->m_datetime.day;
            recur.until.hour = 23;
            recur.until.minute = 59;
            recur.until.second = 59;
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
        for( unsigned int i=0; i<m_exceptiondates.size(); i++ ) {
            icaltimetype exdate = ConvertFromPrtime( m_exceptiondates[i] );
            prop = icalproperty_new_exdate( exdate );
            icalcomponent_add_property( vevent, prop );
        }
    }

    //startdate
    
    if( m_allday ) {
        m_start->SetHour( 0 );
        m_start->SetMinute( 0 );
    }
    prop = icalproperty_new_dtstart( m_start->m_datetime );
    icalcomponent_add_property( vevent, prop );

    //enddate
    if( m_allday ) {
        m_end->SetHour( 23 );
        m_end->SetMinute( 59 );
    }
    prop = icalproperty_new_dtend( m_end->m_datetime );
    icalcomponent_add_property( vevent, prop );

    //snoozetimes
    icalcomponent *tmpcomp=NULL;
    unsigned int i;
    for( i=0; i<m_snoozetimes.size(); i++ ) {
        if( tmpcomp == NULL )
            tmpcomp = icalcomponent_new( ICAL_X_COMPONENT );
        icaltimetype snoozetime = ConvertFromPrtime( m_snoozetimes[i] );
        prop = icalproperty_new_dtstamp( snoozetime );
        icalcomponent_add_property( tmpcomp, prop );
    }
    if( tmpcomp )
        icalcomponent_add_component( vevent, tmpcomp );

    PRUint32 attachmentCount = 0;
    m_attachments->Count(&attachmentCount);
    nsCOMPtr<nsIMsgAttachment> element;
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


    //add event to newcalendar
    icalcomponent_add_component( newcalendar, vevent );
    return newcalendar;
}

/********************************************************************************************/
#include <nsIServiceManager.h>
 
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
    NS_INIT_ISUPPORTS();
    nsresult rv;
    if( event == nsnull ) {
        mEvent = do_CreateInstance(OE_ICALEVENTDISPLAY_CONTRACTID, &rv);
    } else {
        NS_ADDREF( event );
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

NS_IMETHODIMP oeICalEventDisplayImpl::GetEvent( oeIICalEvent **ev )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalEventDisplayImpl::GetEvent()\n" );
#endif
#ifdef ICAL_DEBUG
    printf( "WARNING: .event is no longer needed to access event fields\n" );
#endif
    *ev = mEvent;
    NS_ADDREF(*ev);
    return NS_OK;
}
