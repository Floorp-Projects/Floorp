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

/* This file implements the Container object for calendars. The container is the topmost object in 
the backend and contains and interact with all calendar objects. It acts as a distributor of global
commands to individual calendars and a collector of calculated data for global queries.
*/

#include "oeICalContainerImpl.h"
#include "nsISupportsArray.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMArray.h"
#include "nsISimpleEnumerator.h"

/**
 * This enumerator collects items from an array of other enumerators, and
 * returns it as a single one.
 */
class oeCollectedEventEnumerator : public nsISimpleEnumerator
{
  public:
    NS_DECL_ISUPPORTS

    oeCollectedEventEnumerator(nsCOMArray<nsISimpleEnumerator>& aListOfEnums)
      : mListOfEnums(aListOfEnums),
        mEnumIndex(0)
    {
    }
    
    NS_IMETHOD HasMoreElements(PRBool *aResult) 
    {
      *aResult = PR_FALSE;
      if( mEnumIndex >= mListOfEnums.Count() )
           return NS_OK;
      mListOfEnums[mEnumIndex]->HasMoreElements(aResult);
      while (!*aResult && (++mEnumIndex < mListOfEnums.Count())) {
        mListOfEnums[mEnumIndex]->HasMoreElements(aResult);
      }
      return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **aResult) 
    {
      return mListOfEnums[mEnumIndex]->GetNext(aResult);
    }

    virtual ~oeCollectedEventEnumerator() 
    {
    }

  protected:
    nsCOMArray<nsISimpleEnumerator> mListOfEnums;
    PRInt32 mEnumIndex;
};

NS_IMPL_ISUPPORTS1(oeCollectedEventEnumerator, nsISimpleEnumerator)

                         
icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );
extern "C" {
extern icalarray *builtin_timezones;
extern char *gDefaultTzidPrefix;
struct _icaltimezone {//from icaltimezone.h
    char                *tzid;
    char                *location;
    char                *tznames;
    double              latitude;
    double              longitude;
    icalcomponent       *component;
    icaltimezone        *builtin_timezone;
    int                 end_year;
    icalarray           *changes;
  };
}

oeIICalContainer *gContainer=nsnull;

//icaltimetype ConvertFromPrtime( PRTime indate );
void icaltimezone_init_mozilla_zones (void)
{
    gDefaultTzidPrefix="/Mozilla.org/";

    char timezonecalstr[] = \
"BEGIN:VCALENDAR\n\
PRODID:-//Mozilla.org/NONSGML Mozilla Calendar Timezone Table V1.0//EN\n\
VERSION:2.0\n\
END:VCALENDAR\n\
";

    char *timezoneformatstr = "BEGIN:VTIMEZONE\n\
TZID:/Mozilla.org/BasicTimezones/NH-GMT%c%02d:%02d\n\
LOCATION:NH-GMT%c%02d:%02d\n\
BEGIN:STANDARD\n\
TZOFFSETFROM:%c%02d%02d\n\
TZOFFSETTO:%c%02d%02d\n\
TZNAME:NHS-GMT%c%02d:%02d\n\
DTSTART:19991031T020000\n\
RRULE:FREQ=YEARLY;BYMONTH=10;BYDAY=-1SU\n\
END:STANDARD\n\
BEGIN:DAYLIGHT\n\
TZOFFSETFROM:%c%02d%02d\n\
TZOFFSETTO:%c%02d%02d\n\
TZNAME:NHD-GMT%c%02d:%02d\n\
DTSTART:20000402T020000\n\
RRULE:FREQ=YEARLY;BYMONTH=4;BYMONTHDAY=1,2,3,4,5,6,7;BYDAY=SU\n\
END:DAYLIGHT\n\
END:VTIMEZONE\n";

    char *zulutimezonestr = "BEGIN:VTIMEZONE\n\
TZID:/Mozilla.org/BasicTimezones/GMT\n\
LOCATION:GMT\n\
END:VTIMEZONE\n";

    char timezonestr[1024];

    if( builtin_timezones )
        return;

    builtin_timezones = icalarray_new ( sizeof(icaltimezone), 32);

    icalcomponent *vcalendar = icalparser_parse_string(timezonecalstr);
    icalcomponent *vtimezone;

    //first add the UTC timezone
    vtimezone = icalcomponent_new_from_string( zulutimezonestr );
    icalcomponent_add_component( vcalendar, vtimezone );

    //Components should be sorted by location
    //+00:00 to +12:00
    int i;
    for( i=0; i<25; i++ ) {
        char sign = '+';
        int hour = i/2;
        int minute = i%2==0 ? 00 : 30;
        int hour2 = (i+2)/2;
        sprintf( timezonestr, timezoneformatstr
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign, hour2, minute
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign, hour2, minute
                 , sign, hour, minute );
        vtimezone = icalcomponent_new_from_string( timezonestr );
        icalcomponent_add_component( vcalendar, vtimezone );
    }
    //-00:30 to -12:00
    for( i=-1; i>-25; i-- ) {
        char sign = '-';
        int hour = -1*i/2;
        int minute = i%2==0 ? 00 : 30;
        char sign2 = (i+2)<0 ? '-' : '+';
        int hour2 = (i+2)<0 ? -1*(i+2)/2 : (i+2)/2;
        sprintf( timezonestr, timezoneformatstr
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign2, hour2, minute
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign, hour, minute
                 , sign2, hour2, minute
                 , sign, hour, minute );
        vtimezone = icalcomponent_new_from_string( timezonestr );
        icalcomponent_add_component( vcalendar, vtimezone );
    }

    for( vtimezone = icalcomponent_get_first_component( vcalendar, ICAL_VTIMEZONE_COMPONENT );
         vtimezone != NULL;
         vtimezone = icalcomponent_get_next_component( vcalendar, ICAL_VTIMEZONE_COMPONENT ) ) {
        icaltimezone *zone=icaltimezone_new();
        icaltimezone_get_vtimezone_properties  ( zone, vtimezone );
        icalarray_append (builtin_timezones, zone);
    }

    PRExplodedTime ext;
    PR_ExplodeTime( PR_Now(), PR_LocalTimeParameters, &ext);
    int gmtoffsethour = ext.tm_params.tp_gmt_offset < 0 ? -1*ext.tm_params.tp_gmt_offset / 3600 : ext.tm_params.tp_gmt_offset / 3600;
    int gmtoffsetminute = ext.tm_params.tp_gmt_offset%3600 ? 30 : 00;
    char zone_location[20];
    sprintf( zone_location, "NH-GMT%c%02d:%02d", ext.tm_params.tp_gmt_offset < 0 ? '-' : '+'
                                                , gmtoffsethour, gmtoffsetminute );

    currenttimezone = icaltimezone_get_builtin_timezone ( zone_location );
}

oeICalContainerImpl::oeICalContainerImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::oeICalContainerImpl()\n" );
#endif

        m_batchMode = false;

        NS_NewISupportsArray(getter_AddRefs(m_calendarArray));
        if ( m_calendarArray == nsnull ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalContainerImpl::oeICalContainerImpl() - Warning : Can't create calendar array!\n" );
            #endif
        }

        NS_NewISupportsArray(getter_AddRefs(m_observerArray));
        if ( m_observerArray == nsnull ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalContainerImpl::oeICalContainerImpl() - Warning : Can't create observer array!\n" );
            #endif
        }

        NS_NewISupportsArray(getter_AddRefs(m_todoobserverArray));
        if ( m_todoobserverArray == nsnull ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalContainerImpl::oeICalContainerImpl() - Warning : Can't create todo observer array!\n" );
            #endif
        }

        m_filter = new oeICalContainerFilter();
        NS_ADDREF( m_filter );
        m_filter->m_calendarArray = m_calendarArray;

        icaltimezone_init_mozilla_zones ();

        gContainer = this;
}

oeICalContainerImpl::~oeICalContainerImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::~oeICalContainerImpl()\n" );
#endif

    m_calendarArray->Clear();
    m_calendarArray = nsnull;
    m_observerArray = nsnull;
    m_todoobserverArray = nsnull;

    NS_RELEASE( m_filter );
    gContainer = nsnull;
}

/**
 * NS_IMPL_ISUPPORTS1 expands to a simple implementation of the nsISupports
 * interface.  This includes a proper implementation of AddRef, Release,
 * and QueryInterface.  If this class supported more interfaces than just
 * nsISupports, 
 * you could use NS_IMPL_ADDREF() and NS_IMPL_RELEASE() to take care of the
 * simple stuff, but you would have to create QueryInterface on your own.
 * nsSampleFactory.cpp is an example of this approach.
 * Notice that the second parameter to the macro is the static IID accessor
 * method, and NOT the #defined IID.
 */
NS_IMPL_ISUPPORTS1(oeICalContainerImpl, oeIICalContainer)

NS_IMETHODIMP
oeICalContainerImpl::AddCalendar( const char *server, const char *type ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::AddCalendar(%s, %s)\n", server, type );
#endif

    nsresult rv;

    nsCOMPtr<oeIICal> calendar;
    GetCalendar( server , getter_AddRefs(calendar) );
    if( calendar ) {
        #ifdef ICAL_DEBUG_ALL
        printf( "oeICalContainerImpl::AddCalendar()-Warning: Calendar already exists\n" );
        #endif
        return NS_OK;
    }

    nsCAutoString contractid;
    contractid.Assign(NS_LITERAL_CSTRING("@mozilla.org/ical;1"));
    if (type && strlen(type)) {
      contractid.Append(NS_LITERAL_CSTRING("?type="));
      contractid.Append(type);
    }
    calendar = do_CreateInstance(contractid.get(), &rv);
    if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
    }

    m_calendarArray->AppendElement( calendar );

    PRUint32 num;
    unsigned int i;
    m_observerArray->Count( &num );
    for ( i=0; i<num; i++ ) {
        oeIICalObserver* tmpobserver;
        m_observerArray->GetElementAt( i, (nsISupports **)&tmpobserver );
        calendar->AddObserver( tmpobserver );
    }

    m_todoobserverArray->Count( &num );
    for ( i=0; i<num; i++ ) {
        oeIICalTodoObserver* tmpobserver;
        m_todoobserverArray->GetElementAt( i, (nsISupports **)&tmpobserver );
        calendar->AddTodoObserver( tmpobserver );
    }

    calendar->SetBatchMode( m_batchMode ); //Make sure the current batchmode value is inherited

    calendar->SetServer( server );

    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetCalendar( const char *server, oeIICal **calendar ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetCalendar(%s)\n", server );
#endif
    
    PRUint32 num;
    unsigned int i;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> tmpcalendar;
        char *tmpserver;
        m_calendarArray->GetElementAt( i, getter_AddRefs( tmpcalendar ) );
        tmpcalendar->GetServer( &tmpserver );
        if( strcmp( tmpserver, server ) == 0 ) {
            *calendar = tmpcalendar;
            NS_ADDREF( *calendar );
            nsMemory::Free( tmpserver );
            return NS_OK;
        }
        nsMemory::Free( tmpserver );
    }
    *calendar = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::RemoveCalendar( const char *server ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::RemoveCalendar(%s)\n", server );
#endif
    PRUint32 num;
    unsigned int i;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> tmpcalendar;
        char *tmpserver;
        m_calendarArray->GetElementAt( i, getter_AddRefs( tmpcalendar ) );
        tmpcalendar->GetServer( &tmpserver );
        if( strcmp( tmpserver, server ) == 0 ) {
            nsMemory::Free( tmpserver );
            m_calendarArray->RemoveElementAt( i );
            return NS_OK;
        }
        nsMemory::Free( tmpserver );
    }
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::AddCalendars( PRUint32 serverCount, const char **servers ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::AddCalendars( %d, [Array] )\n", serverCount );
#endif
    nsresult rv=NS_OK;
    for( unsigned int i=0; i<serverCount; i++ ) {
        rv = AddCalendar( servers[i], "" );
        if( NS_FAILED( rv ) )
            break;
    }
    return rv;
}

/* attribute boolean batchMode; */
NS_IMETHODIMP oeICalContainerImpl::GetBatchMode(PRBool *aBatchMode)
{
    *aBatchMode = m_batchMode;
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::SetBatchMode(PRBool aBatchMode)
{
    if( m_batchMode != aBatchMode )
    {
        m_batchMode = aBatchMode;

        // tell observers about the change
        PRUint32 num;
        unsigned int i;

        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
            calendar->SetBatchMode( aBatchMode );
        }

        m_observerArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsresult rv;
            oeIICalObserver* tmpobserver;
            m_observerArray->GetElementAt( i, (nsISupports **)&tmpobserver );
            if( m_batchMode ) {
                rv = tmpobserver->OnStartBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalContainerImpl::SetBatchMode() : WARNING Call to observer's onStartBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }
            else {
                rv = tmpobserver->OnEndBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalContainerImpl::SetBatchMode() : WARNING Call to observer's onEndBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }

        }
        m_todoobserverArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsresult rv;
            oeIICalTodoObserver* tmpobserver;
            m_todoobserverArray->GetElementAt( i, (nsISupports **)&tmpobserver );
            if( m_batchMode ) {
                rv = tmpobserver->OnStartBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalContainerImpl::SetBatchMode() : WARNING Call to observer's onStartBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }
            else {
                rv = tmpobserver->OnEndBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalContainerImpl::SetBatchMode() : WARNING Call to observer's onEndBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }

        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::AddEvent( oeIICalEvent *icalevent, const char *server, char **retid )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::AddEvent()\n" );
#endif
    nsresult rv;
    nsCOMPtr<oeIICal> calendar;
    GetCalendar( server , getter_AddRefs(calendar) );
    if( !calendar ) {
        AddCalendar( server, "" );
        GetCalendar( server , getter_AddRefs(calendar) );
        if( !calendar ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalContainerImpl::AddEvent()-Error cannot find or create calendar\n" );
            #endif
            return NS_ERROR_FAILURE;
        } else {
            rv = calendar->AddEvent( icalevent, retid );
            RemoveCalendar( server );
        }
    } else {
        rv = calendar->AddEvent( icalevent, retid );
    }
    return rv;
}

NS_IMETHODIMP oeICalContainerImpl::ModifyEvent( oeIICalEvent *icalevent, char **retid )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::ModifyEvent()\n" );
#endif
    oeIICal *calendar;
    icalevent->GetParent( &calendar );    
    if( !calendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::ModifyEvent()-Error: Event does not have a parent calendar\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    nsresult rv = calendar->ModifyEvent( icalevent, retid );
    NS_RELEASE( calendar );
    return rv;
}

NS_IMETHODIMP oeICalContainerImpl::FetchEvent( const char *id, oeIICalEvent **ev )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::FetchEvent()\n" );
#endif

    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::FetchEvent() - Invalid Id.\n" );
        #endif
        *ev = nsnull;
        return NS_ERROR_FAILURE;
    }

    PRUint32 num;
    unsigned int i;

    *ev = nsnull;
    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        calendar->FetchEvent( id, ev );
        if( *ev )
            break;
    }

    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::DeleteEvent( const char *id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::DeleteEvent( %s )\n", id );
#endif
    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteEvent() - Invalid Id.\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    oeIICalEvent *event;
    FetchEvent( id , &event );
    if( !event ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteEvent() - Event Not Found.\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    oeIICal *calendar;
    event->GetParent( &calendar );
    NS_RELEASE( event );
    if( !calendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteEvent()-Error: Event does not have a parent calendar\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    nsresult rv = calendar->DeleteEvent( id );
    NS_RELEASE( calendar );

	return rv;
}

NS_IMETHODIMP
oeICalContainerImpl::GetAllEvents(nsISimpleEnumerator **eventlist )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetAllEvents()\n" );
#endif
    
    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetAllEvents( getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetEventsForMonth( PRTime datems, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetEventsForMonth()\n" );
#endif

    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetEventsForMonth( datems, getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetEventsForWeek( PRTime datems, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetEventsForWeek()\n" );
#endif

    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetEventsForWeek( datems, getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetEventsForDay( PRTime datems, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetEventsForDay()\n" );
#endif

    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetEventsForDay( datems, getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::GetEventsForRange()\n" );
#endif
    
    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetEventsForRange( checkdateinms, checkenddateinms, getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalContainerImpl::GetFirstEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
//NOTE: checkenddateinms is being ignored for now
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::GetFirstEventsForRange()\n" );
#endif

    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetFirstEventsForRange( checkdateinms, checkenddateinms, getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }

    *eventlist = new oeCollectedEventEnumerator(listOfEnums);
    if (!*eventlist)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*eventlist);
    return NS_OK;
}

icaltimetype oeICalContainerImpl::GetNextEvent( icaltimetype starting ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::GetNextEvent()\n" );
#endif
    icaltimetype soonest = icaltime_null_time();
    
    PRUint32 num;
    unsigned int i;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> tmpcalendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(tmpcalendar) );
        oeICalImpl *calendar = (oeICalImpl *)(&tmpcalendar);
        icaltimetype next = calendar->GetNextEvent( starting );
        if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
            soonest = next;
        }
    }

    return soonest;
}

NS_IMETHODIMP
oeICalContainerImpl::GetNextNEvents( PRTime datems, PRInt32 maxcount, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetNextNEvents( %d )\n", maxcount );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    int count = 0;
    icaltimetype nextcheckdate;
    do {
        nextcheckdate = GetNextEvent( checkdate );
        if( !icaltime_is_null_time( nextcheckdate )) {

            PRUint32 num;
            unsigned int i;
            m_calendarArray->Count( &num );
            for( i=0; i<num && count<maxcount; i++ ) 
            {
                nsCOMPtr<oeIICal> tmpcalendar;
                m_calendarArray->GetElementAt( i, getter_AddRefs(tmpcalendar) );
                oeICalImpl *calendar = (oeICalImpl *)(&tmpcalendar);
                EventList *tmplistptr = calendar->GetEventList();
                while( tmplistptr && count<maxcount ) {
                    if( tmplistptr->event ) {
                        bool isbeginning,isallday;
                        oeIICalEvent* tmpevent = tmplistptr->event;
                        icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, &isbeginning );
                        isallday = next.is_date;
                        next.is_date = false;
                        if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                            ((oeICalEventImpl *)tmpevent)->ChopAndAddEventToEnum( nextcheckdate, eventlist, isallday, isbeginning );
                            count++;
                        }
                    }
                    tmplistptr = tmplistptr->next;
                }
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) && (count < maxcount) );

    return NS_OK;
}

NS_IMETHODIMP 
oeICalContainerImpl::AddObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::AddObserver()\n" );
#endif
    if( observer ) {
        NS_ADDREF( observer );
        m_observerArray->AppendElement( observer );

        PRUint32 num;
        unsigned int i;
        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs( calendar ) );
            calendar->AddObserver( observer );
        }
        observer->OnLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalContainerImpl::RemoveObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::RemoveObserver()\n" );
#endif
    if( observer ) {
        PRUint32 num;
        unsigned int i;
        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs( calendar ) );
            calendar->RemoveObserver( observer );
        }
        m_observerArray->RemoveElement( observer );
        NS_RELEASE( observer );
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalContainerImpl::AddTodoObserver(oeIICalTodoObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::AddTodoObserver()\n" );
#endif
    if( observer ) {
        NS_ADDREF( observer );
        m_todoobserverArray->AppendElement( observer );

        PRUint32 num;
        unsigned int i;
        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs( calendar ) );
            calendar->AddTodoObserver( observer );
        }
        observer->OnLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalContainerImpl::RemoveTodoObserver(oeIICalTodoObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::RemoveTodoObserver()\n" );
#endif
    if( observer ) {
        PRUint32 num;
        unsigned int i;
        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs( calendar ) );
            calendar->RemoveTodoObserver( observer );
        }
        m_todoobserverArray->RemoveElement( observer );
        NS_RELEASE( observer );
    }
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::AddTodo(oeIICalTodo *icaltodo, const char *server, char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::AddTodo()\n" );
#endif
    nsresult rv;
    nsCOMPtr<oeIICal> calendar;
    GetCalendar( server , getter_AddRefs(calendar) );
    if( !calendar ) {
        AddCalendar( server, "" );
        GetCalendar( server , getter_AddRefs(calendar) );
        if( !calendar ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalContainerImpl::AddTodo()-Error cannot find or create calendar\n" );
            #endif
            return NS_ERROR_FAILURE;
        } else {
            rv = calendar->AddTodo( icaltodo, retid );
            RemoveCalendar( server );
        }
    } else {
        rv = calendar->AddTodo( icaltodo, retid );
    }
    return rv;
}

NS_IMETHODIMP
oeICalContainerImpl::DeleteTodo( const char *id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::DeleteTodo( %s )\n", id );
#endif
    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteTodo() - Invalid Id.\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    oeIICalTodo *todo;
    FetchTodo( id , &todo );
    if( !todo ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteTodo() - Todo Not Found.\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    oeIICal *calendar;
    todo->GetParent( &calendar );
    NS_RELEASE( todo );
    if( !calendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::DeleteTodo()-Error: Todo does not have a parent calendar\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    nsresult rv = calendar->DeleteTodo( id );
    NS_RELEASE( calendar );

	return rv;
}

NS_IMETHODIMP oeICalContainerImpl::FetchTodo( const char *id, oeIICalTodo **ev)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalContainerImpl::FetchTodo()\n" );
#endif

    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::FetchTodo() - Invalid Id.\n" );
        #endif
        *ev = nsnull;
        return NS_ERROR_FAILURE;
    }

    PRUint32 num;
    unsigned int i;

    *ev = nsnull;
    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        calendar->FetchTodo( id, ev );
        if( *ev )
            break;
    }

    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::ModifyTodo(oeIICalTodo *icalevent, char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::ModifyEvent()\n" );
#endif
    oeIICal *calendar;
    icalevent->GetParent( &calendar );    
    if( !calendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalContainerImpl::ModifyEvent()-Error: ToDo does not have a parent calendar\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    nsresult rv = calendar->ModifyTodo( icalevent, retid );
    NS_RELEASE( calendar );
    return rv;
}

NS_IMETHODIMP
oeICalContainerImpl::GetAllTodos(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalContainerImpl::GetAllTodos()\n" );
#endif
    PRUint32 num;
    unsigned int i;
    nsCOMArray<nsISimpleEnumerator> listOfEnums;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs( calendar ) );
        nsCOMPtr<nsISimpleEnumerator> eventEnum;
        calendar->GetAllTodos( getter_AddRefs(eventEnum) );
        listOfEnums.AppendObject(eventEnum);
    }
    
    *resultList = new oeCollectedEventEnumerator(listOfEnums);
    if (!*resultList)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*resultList);
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::ReportError( PRInt16 severity, PRUint32 errorid, const char *errorstring ) {
    PRUint32 num;
    unsigned int i;
    m_observerArray->Count( &num );
    for ( i=0; i<num; i++ ) {
        oeIICalObserver* tmpobserver;
        m_observerArray->GetElementAt( i, (nsISupports **)&tmpobserver );
        tmpobserver->OnError( severity, errorid, errorstring );
    }

    m_todoobserverArray->Count( &num );
    for ( i=0; i<num; i++ ) {
        oeIICalTodoObserver* tmpobserver;
        m_todoobserverArray->GetElementAt( i, (nsISupports **)&tmpobserver );
        tmpobserver->OnError( severity, errorid, errorstring );
    }
    return NS_OK;
}

/*************************************************************************************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/
/*    Filter stuff here                                                                                      */
/*************************************************************************************************************/
/*************************************************************************************************************/

NS_IMETHODIMP oeICalContainerImpl::GetFilter( oeIICalTodo **retval )
{
    *retval = m_filter;
    NS_ADDREF(*retval);
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerImpl::ResetFilter()
{
    PRUint32 num;
    unsigned int i;

    m_calendarArray->Count( &num );
    for( i=0; i<num; i++ ) 
    {
        nsCOMPtr<oeIICal> calendar;
        m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
        calendar->ResetFilter();
    }
    oeIDateTime *completed;
    m_filter->GetCompleted( &completed );
    completed->Clear();
    return NS_OK;
}
  
NS_IMPL_ISUPPORTS1(oeICalContainerFilter, oeIICalTodo)

oeICalContainerFilter::oeICalContainerFilter()
{
    m_completed = new oeFilterDateTime();
    m_completed->SetFieldType( ICAL_COMPLETED_PROPERTY );
    m_completed->m_parent = this;
    NS_ADDREF( m_completed );
}

oeICalContainerFilter::~oeICalContainerFilter()
{
    NS_RELEASE( m_completed );
}

NS_IMETHODIMP oeICalContainerFilter::GetType(Componenttype *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetId(char **aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetId(const char *aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetTitle(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetTitle(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetDescription(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetDescription(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetLocation(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetLocation(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetCategories(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetCategories(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetUrl(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetUrl(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetPrivateEvent(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetPrivateEvent(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetMethod(eventMethodProperty *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetMethod(eventMethodProperty aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetStatus(eventStatusProperty *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetStatus(eventStatusProperty aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetPriority(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetPriority(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetSyncId(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetSyncId(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAllDay(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalContainerFilter::SetAllDay(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAlarm(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetAlarm(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAlarmUnits(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetAlarmUnits(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAlarmLength(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetAlarmLength(PRUint32 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAlarmEmailAddress(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetAlarmEmailAddress(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetInviteEmailAddress(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetInviteEmailAddress(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurInterval(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetRecurInterval(PRUint32 aNewVal )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurCount(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetRecurCount(PRUint32 aNewVal )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurUnits(char **aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetRecurUnits(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecur(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetRecur(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurForever(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalContainerFilter::SetRecurForever(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetLastModified(PRTime *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::UpdateLastModified()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetLastAlarmAck(PRTime *aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetLastAlarmAck(PRTime aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetNextRecurrence( PRTime begin, PRTime *retval, PRBool *isvalid ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetPreviousOccurrence( PRTime beforethis, PRTime *retval, PRBool *isvalid ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetStart(oeIDateTime * *start)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetEnd(oeIDateTime * *end)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetStamp(oeIDateTime * *stamp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurEnd(oeIDateTime * *recurend)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurWeekdays(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalContainerFilter::SetRecurWeekdays(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetRecurWeekNumber(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetRecurWeekNumber(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetIcalString(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

	NS_IMETHODIMP oeICalContainerFilter::ParseIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::AddException( PRTime exdate )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::RemoveAllExceptions()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetExceptions(nsISimpleEnumerator **datelist )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetSnoozeTime( PRTime snoozetime )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::Clone( oeIICalEvent **ev )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetDue(oeIDateTime * *due)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetCompleted(oeIDateTime * *completed)
{
    *completed = m_completed;
    NS_ADDREF(*completed);
    return NS_OK;
}

NS_IMETHODIMP oeICalContainerFilter::GetPercent(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetPercent(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::AddAttachment(nsIMsgAttachment *attachment)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::RemoveAttachment(nsIMsgAttachment *attachment)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::RemoveAttachments()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetAttachmentsArray(nsISupportsArray * *aAttachmentsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::AddContact(nsIAbCard *contact)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::RemoveContact(nsIAbCard *contact)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::RemoveContacts()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetContactsArray(nsISupportsArray * *aContactsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetParent( oeIICal *parent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetParent( oeIICal **parent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetTodoIcalString(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::ParseTodoIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetDuration(PRBool is_negative, PRUint16 weeks, PRUint16 days, PRUint16 hours, PRUint16 minutes, PRUint16 seconds)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetDuration(PRBool *is_negative, PRUint16 *weeks, PRUint16 *days, PRUint16 *hours, PRUint16 *minutes, PRUint16 *seconds)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void oeICalContainerFilter::UpdateAllFilters( PRInt32 icaltype )
{
    switch ( icaltype ) {
    case ICAL_COMPLETED_PROPERTY:
        PRTime completedvalue;
        m_completed->GetTime( &completedvalue );

        PRUint32 num;
        unsigned int i;

        m_calendarArray->Count( &num );
        for( i=0; i<num; i++ ) 
        {
            nsCOMPtr<oeIICal> calendar;
            m_calendarArray->GetElementAt( i, getter_AddRefs(calendar) );
            oeIICalTodo *filter;
            //XXX comptr? getter_AddRefs?
            calendar->GetFilter( &filter );
            if (filter) {
                oeIDateTime *completed;
                filter->GetCompleted( &completed );
                completed->SetTime( completedvalue );
            }
        }

        break;
    default:
        break;
    }
}

NS_IMETHODIMP oeICalContainerFilter::ReportError( PRInt16 severity, PRUint32 errorid, const char *errorstring ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::SetParameter( const char *name, const char *value ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalContainerFilter::GetParameter( const char *name, char **value ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////
// FilterDateTime
//////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(oeFilterDateTime, oeIDateTime)

oeFilterDateTime::oeFilterDateTime()
{
  m_datetime = icaltime_null_time();
}

oeFilterDateTime::~oeFilterDateTime()
{
}

NS_IMETHODIMP oeFilterDateTime::GetYear(PRInt16 *retval)
{
    *retval = m_datetime.year;
    return NS_OK;
}
NS_IMETHODIMP oeFilterDateTime::SetYear(PRInt16 newval)
{
    m_datetime.year = newval;
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetMonth(PRInt16 *retval)
{
    *retval = m_datetime.month - 1;
    return NS_OK;
}
NS_IMETHODIMP oeFilterDateTime::SetMonth(PRInt16 newval)
{
    m_datetime.month = newval + 1;
    if( m_datetime.month < 1 || m_datetime.month > 12 )
        m_datetime = icaltime_normalize( m_datetime );
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetDay(PRInt16 *retval)
{
    *retval = m_datetime.day;
    return NS_OK;
}
NS_IMETHODIMP oeFilterDateTime::SetDay(PRInt16 newval)
{
    m_datetime.day = newval;
    if( newval < 1 || newval > 31 )
        m_datetime = icaltime_normalize( m_datetime );
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetHour(PRInt16 *retval)
{
    *retval = m_datetime.hour;
    return NS_OK;
}
NS_IMETHODIMP oeFilterDateTime::SetHour(PRInt16 newval)
{
    m_datetime.hour = newval;
    if( newval < 0 || newval > 23 )
        m_datetime = icaltime_normalize( m_datetime );
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetMinute(PRInt16 *retval)
{
    *retval = m_datetime.minute;
    return NS_OK;
}
NS_IMETHODIMP oeFilterDateTime::SetMinute(PRInt16 newval)
{
    m_datetime.minute = newval;
    if( newval < 0 || newval > 59 )
        m_datetime = icaltime_normalize( m_datetime );
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetTime(PRTime *retval)
{
    *retval = ConvertToPrtime( m_datetime );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::ToString(char **retval)
{
    char tmp[20];
    sprintf( tmp, "%04d/%02d/%02d %02d:%02d:%02d" , m_datetime.year, m_datetime.month, m_datetime.day, m_datetime.hour, m_datetime.minute, m_datetime.second );
    *retval= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::SetTime( PRTime ms )
{
	m_datetime = ConvertFromPrtime( ms );
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::Clear()
{
	m_datetime = icaltime_null_time();
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::GetUtc(PRBool *retval)
{
    *retval = m_datetime.is_utc;
    return NS_OK;
}

NS_IMETHODIMP oeFilterDateTime::SetUtc(PRBool newval)
{
    m_datetime.is_utc = newval;
    m_parent->UpdateAllFilters( m_icaltype );
    return NS_OK;
}

void oeFilterDateTime::SetFieldType( PRInt32 icaltype )
{
    m_icaltype = icaltype;
}

NS_IMETHODIMP oeFilterDateTime::GetTzID(char **aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeFilterDateTime::SetTimeInTimezone( PRTime ms, const char *tzid )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

