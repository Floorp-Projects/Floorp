/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Mostafa Hosseini (mostafah@oeone.com)
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
 * 
*/

/* This file implements a date-time XPCOM object used to pass date-time values between the frontend and the
backend. It provides access to individual date-time fields from javascript and translates that data to a 
icaltimetype structure for the backend.*/

#ifndef WIN32
#include <unistd.h>
#endif

#include "oeDateTimeImpl.h"
#include "nsMemory.h"
#include "stdlib.h"

icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );
icaltimezone *currenttimezone = nsnull;

/* Implementation file */
NS_IMPL_ISUPPORTS1(oeDateTimeImpl, oeIDateTime)

nsresult
NS_NewDateTime( oeIDateTime** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeDateTimeImpl();
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeDateTimeImpl::oeDateTimeImpl()
{
  /* member initializers and constructor code */
  m_datetime = icaltime_null_time();
  m_tzid = nsnull;
}

oeDateTimeImpl::~oeDateTimeImpl()
{
    if( m_tzid )
        nsMemory::Free( m_tzid );
}

/* attribute short year; */
NS_IMETHODIMP oeDateTimeImpl::GetYear(PRInt16 *retval)
{
    *retval = m_datetime.year;
    return NS_OK;
}
NS_IMETHODIMP oeDateTimeImpl::SetYear(PRInt16 newval)
{
    m_datetime.year = newval;
    return NS_OK;
}

/* attribute short month; */
NS_IMETHODIMP oeDateTimeImpl::GetMonth(PRInt16 *retval)
{
    *retval = m_datetime.month - 1;
    return NS_OK;
}
NS_IMETHODIMP oeDateTimeImpl::SetMonth(PRInt16 newval)
{
    m_datetime.month = newval + 1;
    if( m_datetime.month < 1 || m_datetime.month > 12 )
        m_datetime = icaltime_normalize( m_datetime );
    return NS_OK;
}

/* attribute short day; */
NS_IMETHODIMP oeDateTimeImpl::GetDay(PRInt16 *retval)
{
    *retval = m_datetime.day;
    return NS_OK;
}
NS_IMETHODIMP oeDateTimeImpl::SetDay(PRInt16 newval)
{
    m_datetime.day = newval;
    if( newval < 1 || newval > 31 )
        m_datetime = icaltime_normalize( m_datetime );
    return NS_OK;
}

/* attribute short hour; */
NS_IMETHODIMP oeDateTimeImpl::GetHour(PRInt16 *retval)
{
    *retval = m_datetime.hour;
    return NS_OK;
}
NS_IMETHODIMP oeDateTimeImpl::SetHour(PRInt16 newval)
{
    m_datetime.hour = newval;
    if( newval < 0 || newval > 23 )
        m_datetime = icaltime_normalize( m_datetime );
    return NS_OK;
}

/* attribute short minute; */
NS_IMETHODIMP oeDateTimeImpl::GetMinute(PRInt16 *retval)
{
    *retval = m_datetime.minute;
    return NS_OK;
}
NS_IMETHODIMP oeDateTimeImpl::SetMinute(PRInt16 newval)
{
    m_datetime.minute = newval;
    if( newval < 0 || newval > 59 )
        m_datetime = icaltime_normalize( m_datetime );
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::GetTime(PRTime *retval)
{
    *retval = ConvertToPrtime( m_datetime );
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::ToString(char **retval)
{
    char tmp[20];
    sprintf( tmp, "%04d/%02d/%02d %02d:%02d:%02d" , m_datetime.year, m_datetime.month, m_datetime.day, m_datetime.hour, m_datetime.minute, m_datetime.second );
    *retval= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::SetTime( PRTime ms )
{
	m_datetime = ConvertFromPrtime( ms );
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::SetTimeInTimezone( PRTime ms, const char *tzid )
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetTimeInTimezone( %s )\n", tzid );
#endif
    if( m_tzid )
        nsMemory::Free( m_tzid );
    
    if( tzid )
        m_tzid= (char*) nsMemory::Clone( tzid, strlen(tzid)+1);
    else
        m_tzid = nsnull;

	icaltimetype newdatetime = ConvertFromPrtime( ms );

    icaltimezone *from_zone = icaltimezone_get_builtin_timezone_from_tzid ( tzid );
    if( from_zone )
        icaltimezone_convert_time ( &newdatetime, from_zone, currenttimezone );
    else {
        if( m_tzid )
            nsMemory::Free( m_tzid );
        m_tzid = nsnull;
    }

    m_datetime = newdatetime;

    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::Clear()
{
    m_datetime = icaltime_null_time();
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::GetIsSet(PRBool *retval)
{
    *retval = ! icaltime_is_null_time(m_datetime);
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::GetUtc(PRBool *retval)
{
    *retval = m_datetime.is_utc;
    return NS_OK;
}

NS_IMETHODIMP oeDateTimeImpl::SetUtc(PRBool newval)
{
    m_datetime.is_utc = newval;
    return NS_OK;
}

void oeDateTimeImpl::AdjustToWeekday( short weekday ) {
    short currentday = icaltime_day_of_week( m_datetime );
    while( currentday != weekday ) {
        m_datetime.day++;
        m_datetime = icaltime_normalize( m_datetime );
        currentday = icaltime_day_of_week( m_datetime );
    }
}

/* readonly attribute string tzid; */
NS_IMETHODIMP oeDateTimeImpl::GetTzID(char **aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetTzID() = " );
#endif
    if( m_tzid ) {
        *aRetVal= (char*) nsMemory::Clone( m_tzid, strlen(m_tzid)+1);
        if( *aRetVal == nsnull )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= nsnull;
#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", *aRetVal );
#endif
    return NS_OK;
}

void oeDateTimeImpl::SetTzID(const char *aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetTzID( %s )\n", aNewVal );
#endif
    if( m_tzid )
        nsMemory::Free( m_tzid );
    
    if( aNewVal )
        m_tzid= (char*) nsMemory::Clone( aNewVal, strlen(aNewVal)+1);
    else
        m_tzid = nsnull;
}

int oeDateTimeImpl::CompareDate( oeDateTimeImpl *anotherdt ) {
    if( m_datetime.year == anotherdt->m_datetime.year &&
        m_datetime.month == anotherdt->m_datetime.month &&
        m_datetime.day == anotherdt->m_datetime.day )
        return 0;
    return 1;
}

