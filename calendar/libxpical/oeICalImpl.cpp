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

#ifndef WIN32
#include <unistd.h>
#endif

#define __cplusplus__ 1

#include <vector>
#include "oeICalImpl.h"
#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "stdlib.h"   
#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
                             
extern "C" {
    #include "icalss.h"
}

icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );
void AlarmTimerCallback(nsITimer *aTimer, void *aClosure);

/* event enumerator */
class
oeEventEnumerator : public nsISimpleEnumerator
{
  public:
    oeEventEnumerator();
    virtual ~oeEventEnumerator();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator interface
    NS_DECL_NSISIMPLEENUMERATOR

    NS_IMETHOD Init( oeIICal* iCal );
    NS_IMETHOD AddEventId( PRInt32 id );

  private:
    PRUint32 mCurrentIndex;
    std::vector<PRInt32> mIdVector;
    nsCOMPtr<oeIICal> mICal;
};

oeEventEnumerator::oeEventEnumerator( ) 
:
    mCurrentIndex( 0 )
{
    NS_INIT_REFCNT();
}

oeEventEnumerator::~oeEventEnumerator()
{
}

NS_IMPL_ISUPPORTS1(oeEventEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeEventEnumerator::HasMoreElements(PRBool *result)
{
    if( mCurrentIndex >= mIdVector.size() )
    {
        *result = PR_FALSE;
    }
    else
    {
        *result = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
oeEventEnumerator::GetNext(nsISupports **_retval)
{
        
    if( mCurrentIndex >= mIdVector.size() )
    {
        *_retval = nsnull;
    }
    else
    {
        PRInt32 eventId = mIdVector[ mCurrentIndex ];
        
        oeIICalEvent* event;      
        nsresult rv = mICal->FetchEvent( eventId, &event);
        if( NS_FAILED( rv ) ) 
           return rv;

        *_retval = event;
        ++mCurrentIndex;
    }
    
    return NS_OK;
}


NS_IMETHODIMP
oeEventEnumerator::AddEventId(PRInt32 id)
{
    mIdVector.push_back( id );
    return NS_OK;
}

NS_IMETHODIMP
oeEventEnumerator::Init(oeIICal* iCal)
{
    mICal = iCal;
    return NS_OK;
}


/* date enumerator */
oeDateEnumerator::oeDateEnumerator( ) 
:
    mCurrentIndex( 0 )
{
    NS_INIT_REFCNT();
}

oeDateEnumerator::~oeDateEnumerator()
{
}

NS_IMPL_ISUPPORTS1(oeDateEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeDateEnumerator::HasMoreElements(PRBool *result)
{
    if( mCurrentIndex >= mIdVector.size() )
    {
        *result = PR_FALSE;
    }
    else
    {
        *result = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeDateEnumerator::GetNext(nsISupports **_retval) 
{ 

    if( mCurrentIndex >= mIdVector.size() ) 
    { 
        *_retval = nsnull; 
    } 
    else 
    { 
        nsresult rv; 
        nsCOMPtr<nsISupportsPRTime> nsPRTime = do_CreateInstance( NS_SUPPORTS_PRTIME_CONTRACTID , &rv); 
        if( NS_FAILED( rv ) ) { 
            *_retval = nsnull; 
            return rv; 
        } 
        nsISupportsPRTime* date; 
        rv = nsPRTime->QueryInterface(NS_GET_IID(nsISupportsPRTime), (void **)&date); 
        if( NS_FAILED( rv ) ) { 
            *_retval = nsnull; 
            return rv; 
        } 

        date->SetData( mIdVector[ mCurrentIndex ] ); 
        *_retval = date; 
        ++mCurrentIndex; 
    } 

    return NS_OK; 
} 

NS_IMETHODIMP
oeDateEnumerator::AddDate(PRTime date)
{
    mIdVector.push_back( date );
    return NS_OK;
}







//////////////////////////////////////////////////
//   ICal Factory
//////////////////////////////////////////////////

nsresult
NS_NewICal( oeIICal** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeICalImpl();
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeICalImpl::oeICalImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::oeICalImpl()\n" );
#endif
        NS_INIT_REFCNT();

        m_batchMode = false;

        m_alarmtimer = nsnull;
}

oeICalImpl::~oeICalImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::~oeICalImpl()\n" );
#endif
    for( int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->Release();
    }
    if( m_alarmtimer  ) {
        if ( m_alarmtimer->GetDelay() != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }
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
NS_IMPL_ISUPPORTS1(oeICalImpl, oeIICal);

/**
*
*   Test
*
*   DESCRIPTION: An exported XPCOM function used by the tester program to make sure
*		 all main internal functions are working properly
*   PARAMETERS: 
*	None
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::Test(void)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::Test()\n" );
#endif
    icalfileset *stream;
    icalcomponent *icalcalendar,*icalevent;
    struct icaltimetype start, end;
    char uidstr[10]="900000000";
    char icalrawcalendarstr[] = "BEGIN:VCALENDAR\n\
BEGIN:VEVENT\n\
UID:900000000\n\
END:VEVENT\n\
END:VCALENDAR\n\
";
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }

    icalcalendar = icalparser_parse_string(icalrawcalendarstr);
    if ( !icalcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot create VCALENDAR!\n" );
        #endif
        return NS_OK;
    }

    icalevent = icalcomponent_get_first_component(icalcalendar,ICAL_VEVENT_COMPONENT);
    if ( !icalevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }

    icalproperty *uid = icalproperty_new_uid( uidstr );
    icalcomponent_add_property( icalevent, uid );

    icalproperty *title = icalproperty_new_summary( "Lunch time" );
    icalcomponent_add_property( icalevent, title );
    
    icalproperty *description = icalproperty_new_description( "Will be out for one hour" );
    icalcomponent_add_property( icalevent, description );
    
    icalproperty *location = icalproperty_new_location( "Restaurant" );
    icalcomponent_add_property( icalevent, location );
    
    icalproperty *category = icalproperty_new_categories( "Personal" );
    icalcomponent_add_property( icalevent, category );

    icalproperty *classp = icalproperty_new_class( ICAL_CLASS_PRIVATE );
    icalcomponent_add_property( icalevent, classp );

    start.year = 2001; start.month = 8; start.day = 15;
    start.hour = 12; start.minute = 24; start.second = 0;
    start.is_utc = false; start.is_date = false;

    end.year = 2001; end.month = 8; end.day = 15;
    end.hour = 13; end.minute = 24; end.second = 0;
    end.is_utc = false; end.is_date = false;

    icalproperty *dtstart = icalproperty_new_dtstart( start );
    icalproperty *dtend = icalproperty_new_dtend( end );
    
    icalcomponent_add_property( icalevent, dtstart );
    icalcomponent_add_property( icalevent, dtend );
    //
    icalproperty *xprop = icalproperty_new_x( "TRUE" );
    icalparameter *xpar = icalparameter_new_member( "AllDay" ); //NOTE: new_member is used because new_x didn't work
                                                                //     Better to be changed to new_x when problem is solved
    icalproperty_add_parameter( xprop, xpar );
    
    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "Alarm" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "AlarmWentOff" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "5" );
    xpar = icalparameter_new_member( "AlarmLength" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "mostafah@oeone.com" );
    xpar = icalparameter_new_member( "AlarmEmailAddres" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "mostafah@oeone.com" );
    xpar = icalparameter_new_member( "InviteEmailAddres" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "5" );
    xpar = icalparameter_new_member( "SnoozeTime" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "0" );
    xpar = icalparameter_new_member( "RecurType" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "0" );
    xpar = icalparameter_new_member( "RecurInterval" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "days" );
    xpar = icalparameter_new_member( "RepeatUnits" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //
    xprop = icalproperty_new_x( "FALSE" );
    xpar = icalparameter_new_member( "RepeatForever" );

    icalproperty_add_parameter( xprop, xpar );

    icalcomponent_add_property( icalevent, xprop );
    //

    struct icaltriggertype trig;
    icalcomponent *valarm = icalcomponent_new_valarm();
    trig.time.year = trig.time.month = trig.time.day = trig.time.hour = trig.time.minute = trig.time.second = 0;
    trig.duration.is_neg = true;
    trig.duration.days = trig.duration.weeks = trig.duration.hours = trig.duration.seconds = 0;
    trig.duration.minutes = 1;
    xprop = icalproperty_new_trigger( trig );
    icalcomponent_add_property( valarm, xprop );
    icalcomponent_add_component( icalevent, valarm );

    icalfileset_add_component(stream,icalcalendar);

	icalfileset_commit(stream);
#ifdef ICAL_DEBUG
	printf("Appended Event\n");
#endif
    
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    if ( !fetchedcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot fetch event!\n" );
        #endif
        return NS_OK;
    }
	
    icalcomponent *fetchedevent = icalcomponent_get_first_component( fetchedcal,ICAL_VEVENT_COMPONENT);
    if ( !fetchedevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }

#ifdef ICAL_DEBUG
    printf("Fetched Event\n");
#endif
	
    icalproperty *tmpprop;
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_UID_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: UID not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("id: %s\n", icalproperty_get_uid( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_SUMMARY_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: SUMMARY not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Title: %s\n", icalproperty_get_summary( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CATEGORIES_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: CATEGORIES not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Category: %s\n", icalproperty_get_categories( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DESCRIPTION_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DESCRIPTION not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Description: %s\n", icalproperty_get_description( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_LOCATION_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: LOCATION not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Location: %s\n", icalproperty_get_location( tmpprop ) );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CLASS_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: CLASS not found!\n" );
        #endif
        return NS_OK;
    }
#ifdef ICAL_DEBUG
    printf("Class: %s\n", (icalproperty_get_class( tmpprop ) == ICAL_CLASS_PUBLIC) ? "PUBLIC" : "PRIVATE" );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTSTART_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DTSTART not found!\n" );
        #endif
        return NS_OK;
    }
    start = icalproperty_get_dtstart( tmpprop );
#ifdef ICAL_DEBUG
    printf("Start: %d-%d-%d %d:%d\n", start.year, start.month, start.day, start.hour, start.minute );
#endif
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTEND_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: DTEND not found!\n" );
        #endif
        return NS_OK;
    }
    end = icalproperty_get_dtstart( tmpprop );
#ifdef ICAL_DEBUG
    printf("End: %d-%d-%d %d:%d\n", end.year, end.month, end.day, end.hour, end.minute );
#endif
    //
    for( tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_X_PROPERTY );
            tmpprop != 0 ;
            tmpprop = icalcomponent_get_next_property( fetchedevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( tmpprop, ICAL_MEMBER_PARAMETER );
#ifdef ICAL_DEBUG
            printf("%s: %s\n", icalparameter_get_member( tmppar ), icalproperty_get_value_as_string( tmpprop ) );
#endif
    }

    icalcomponent *newcomp = icalcomponent_new_clone( fetchedcal );
    if ( !newcomp ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot clone event!\n" );
        #endif
        return NS_OK;
    }
    icalcomponent *newevent = icalcomponent_get_first_component( newcomp, ICAL_VEVENT_COMPONENT );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_OK;
    }
    tmpprop = icalcomponent_get_first_property( newevent, ICAL_SUMMARY_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: SUMMARY not found!\n" );
        #endif
        return NS_OK;
    }
    icalproperty_set_summary( tmpprop, "LUNCH AND LEARN TIME" );
//    icalfileset_modify( stream, fetchedcal, newcomp );
    icalfileset_remove_component( stream, fetchedcal );
    icalfileset_add_component( stream, newcomp );
    icalcomponent_free( fetchedcal );
    //

#ifdef ICAL_DEBUG
	printf("Event updated\n");
    printf( "New Title: %s\n", icalproperty_get_summary( tmpprop ) );
#endif

    fetchedcal = icalfileset_fetch( stream, uidstr );
    if ( !fetchedcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot fetch event!\n" );
        #endif
        return NS_OK;
    }
    icalfileset_remove_component( stream, fetchedcal );
	
#ifdef ICAL_DEBUG
    printf("Removed Event\n");
#endif

    icalfileset_commit(stream);
    icalfileset_free(stream);
    return NS_OK;                                                                    
}

/**
*
*   SetServer
*
*   DESCRIPTION: Sets the Calendar address string of the server.
*   PARAMETERS: 
*	str	- The calendar address string
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::SetServer( const char *str ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SetServer(%s)\n", str );
#endif
	strcpy( serveraddr, str );

    icalfileset *stream;
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SetServer() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        
        nsresult rv;
        oeICalEventImpl *icalevent;
        if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &icalevent ))) {
            return rv;
        }
        
        icalevent->ParseIcalComponent( comp );

        m_eventlist.Add( icalevent );
    }
    
    icalfileset_free(stream);
    
    for( int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnLoad();
    }
    
    SetupAlarmManager();

    return NS_OK;
}


/* attribute boolean batchMode; */
NS_IMETHODIMP oeICalImpl::GetBatchMode(PRBool *aBatchMode)
{
    *aBatchMode = m_batchMode;

    return NS_OK;
}


NS_IMETHODIMP oeICalImpl::SetBatchMode(PRBool aBatchMode)
{
    bool newBatchMode = aBatchMode;

    if( m_batchMode != newBatchMode )
    {
        m_batchMode = aBatchMode;

        // if batch mode changed to off, call set up alarm mamnager

        if( !m_batchMode )
        {
            SetupAlarmManager();
        }
        
        // tell observers about the change

        for( int i=0; i<m_observerlist.size(); i++ ) 
        {
            if( m_batchMode )
                m_observerlist[i]->OnStartBatch();
            else
                m_observerlist[i]->OnEndBatch();

        }
    }
    
    return NS_OK;
}


/**
*
*   AddEvent
*
*   DESCRIPTION: Adds an event
*
*   PARAMETERS: 
*	icalcalendar - The XPCOM component representing an event
*	retid   - Place to store the returned id
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::AddEvent(oeIICalEvent *icalevent, PRInt32 *retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddEvent()\n" );
#endif
	unsigned long newid;
    icalfileset *stream;
    icalcomponent *vcalendar,*fetchedcal;
    char uidstr[10];

	*retid = 0;
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::AddEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    ((oeICalEventImpl *)icalevent)->GetId( (PRUint32 *)&newid );

    if( newid == 0 ) {
        do {
            newid = 900000000 + ((rand()%0x4ff) << 16) + rand()%0xFFFF;
            sprintf( uidstr, "%lu", newid );
        } while ( (fetchedcal = icalfileset_fetch( stream, uidstr )) != 0 );

        ((oeICalEventImpl *)icalevent)->SetId( newid );
    }

    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
	
    icalfileset_add_component( stream, vcalendar );
	*retid = newid;

    icalfileset_commit( stream );
    icalfileset_free( stream );

    icalevent->AddRef();
    m_eventlist.Add( icalevent );

    for( int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnAddItem( icalevent );
    }

    SetupAlarmManager();
    return NS_OK;
}

/**
*
*   ModifyEvent
*
*   DESCRIPTION: Updates an event
*
*   PARAMETERS: 
*	icalcalendar	- The XPCOM component representing an event
*	retid   - Place to store the returned id
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::ModifyEvent(oeIICalEvent *icalevent, PRInt32 *retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::ModifyEvent()\n" );
#endif
	unsigned long id;
    icalfileset *stream;
    icalcomponent *vcalendar;
    char uidstr[10];

    *retid = 0;
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    ((oeICalEventImpl *)icalevent)->GetId( (PRUint32 *)&id );
    
    sprintf( uidstr, "%lu", id );
    
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    
    oeICalEventImpl *oldevent=nsnull;
    if( fetchedcal ) {
        icalfileset_remove_component( stream, fetchedcal );
        nsresult rv;
        if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &oldevent ))) {
            icalfileset_free(stream);
            return rv;
        }
        oldevent->ParseIcalComponent( fetchedcal );
        icalcomponent_free( fetchedcal );
    } else {
#ifdef ICAL_DEBUG
    printf( "WARNING Event not found.\n" );
#endif
        icalfileset_free(stream);
        return NS_OK;
    }
    
    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
    icalfileset_add_component( stream, vcalendar );
    
    *retid = id;
	
    icalfileset_commit( stream );
    icalfileset_free(stream);
    
    for( int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnModifyItem( icalevent, oldevent );
    }

    oldevent->Release();

    SetupAlarmManager();
    return NS_OK;
}


/**
*
*   FetchEvent
*
*   DESCRIPTION: Fetches an event
*
*   PARAMETERS: 
*	ev	- Place to store the XPCOM representation of the fetched event
*	id      - Id of the event to fetch
*
*   RETURN
*      None
*/
NS_IMETHODIMP oeICalImpl::FetchEvent(PRInt32 id, oeIICalEvent **ev)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::FetchEvent()\n" );
#endif
    oeIICalEvent* event = m_eventlist.GetEventById( id );
    if( event != nsnull ) {
        event->AddRef();
    }
    *ev = event;
    return NS_OK;
}

/**
*
*   DeleteEvent
*
*   DESCRIPTION: Deletes an event
*
*   PARAMETERS: 
*	id		- Id of the event to delete
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::DeleteEvent( PRInt32 id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::DeleteEvent( %lu )\n", id );
#endif
    icalfileset *stream;
    char uidstr[10];
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    sprintf( uidstr, "%lu", id );

    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    
    if( fetchedcal == 0 ) {
        icalfileset_free(stream);
#ifdef ICAL_DEBUG
    printf( "WARNING Event not found.\n" );
#endif
        return NS_OK;
    }
    
    icalfileset_remove_component( stream, fetchedcal );
	icalcomponent_free( fetchedcal );
	
    icalfileset_commit( stream );
    icalfileset_free(stream);
	
    oeIICalEvent *icalevent;
    FetchEvent( id , &icalevent );

    m_eventlist.Remove( id );
    
    for( int i=0; i<m_observerlist.size(); i++ ) {
        m_observerlist[i]->OnDeleteItem( icalevent );
    }

    icalevent->Release();

    SetupAlarmManager();
	return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetAllEvents(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetAllEvents()\n" );
#endif
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator();
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    eventEnum->Init( this );
    
    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            PRUint32 tmpid;
            tmplistptr->event->GetId( &tmpid );
            eventEnum->AddEventId( tmpid );
        }
        tmplistptr = tmplistptr->next;
    }

    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
}

NS_IMETHODIMP
oeICalImpl::SearchBySQL( const char *sqlstr,  nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SearchBySQL()\n" );
#endif
    icalgauge* gauge;
    icalfileset *stream;
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    eventEnum->Init( this );
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SearchBySQL() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    gauge = icalgauge_new_from_sql( (char *)sqlstr );
    if ( !gauge ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SearchBySQL() failed: Cannot create gauge\n" );
        #endif
        return NS_OK;
    }
    
    icalfileset_select( stream, gauge );

    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        icalcomponent *vevent = icalcomponent_get_first_component( comp, ICAL_VEVENT_COMPONENT );
        if ( !vevent ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SearchBySQL() failed: VEVENT not found!\n" );
            #endif
            return NS_OK;
        }

        icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
        if ( !prop ) {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SearchBySQL() failed: UID not found!\n" );
            #endif
            return NS_OK;
        }

        eventEnum->AddEventId( atol( icalproperty_get_uid( prop ) ) );
    }
	
    icalfileset_free(stream);
    
    // bump ref count
    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
}

NS_IMETHODIMP
oeICalImpl::GetEventsForMonth( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForMonth()\n" );
#endif
    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.day = 1;
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0 , 0, -1 );
#ifdef ICAL_DEBUG_ALL
    printf( "CHECKDATE: %s\n" , icaltime_as_ctime( checkdate ) );
#endif
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.month++;
    checkenddate.day = 1;
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    checkenddate = icaltime_normalize( checkenddate );
#ifdef ICAL_DEBUG_ALL
    printf( "CHECKENDDATE: %s\n" , icaltime_as_ctime( checkenddate ) );
#endif
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForWeek( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForWeek()\n" );
#endif

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0 , 0, -1 );
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    icaltime_adjust( &checkenddate, 7, 0 , 0, 0 );
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForDay( PRTime datems, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetEventsForDay()\n" );
#endif

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    PRTime checkdateinms = ConvertToPrtime( checkdate );

    struct icaltimetype checkenddate = ConvertFromPrtime( datems );
    checkenddate.hour = 0;
    checkenddate.minute = 0;
    checkenddate.second = 0;
    icaltime_adjust( &checkenddate, 1, 0, 0, 0 );
    PRTime checkenddateinms = ConvertToPrtime( checkenddate );

    return GetEventsForRange(checkdateinms ,checkenddateinms ,datelist ,eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetEventsForRange()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator( );
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    eventEnum->Init( this );

    EventList *tmplistptr = &m_eventlist;
    int i=0;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            PRBool isvalid;
            PRTime checkdateloop = checkdateinms;
            tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            while( isvalid && LL_CMP( checkdateloop, <, checkenddateinms ) ) {
                PRUint32 tmpid;
                tmpevent->GetId( &tmpid );
                eventEnum->AddEventId( tmpid );
                dateEnum->AddDate( checkdateloop );
                tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            }
        }
        tmplistptr = tmplistptr->next;
        i++;
    }

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetNextNEvents( PRTime datems, PRInt32 maxcount, nsISimpleEnumerator **datelist, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetNextNEvents()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum = new oeEventEnumerator( );
    
    if (!eventEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    nsCOMPtr<oeDateEnumerator> dateEnum = new oeDateEnumerator( );
    
    if (!dateEnum)
        return NS_ERROR_OUT_OF_MEMORY;

    eventEnum->Init( this );

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    checkdate.hour = 0;
    checkdate.minute = 0;
    checkdate.second = 0;
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    PRTime checkdateinms = ConvertToPrtime( checkdate );
    
    EventList *tmplistptr = &m_eventlist;
    int i=0,count=0;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            PRBool isvalid;
            PRTime checkdateloop = checkdateinms;
            tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            while( isvalid ) {
                PRUint32 tmpid;
                tmpevent->GetId( &tmpid );
                eventEnum->AddEventId( tmpid );
                dateEnum->AddDate( checkdateloop );
                count++;
                if( count == maxcount )
                    break;
                tmpevent->GetNextRecurrence( checkdateloop, &checkdateloop, &isvalid );
            }
        }
        tmplistptr = tmplistptr->next;
        i++;
    }

    eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    dateEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)datelist);
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::AddObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddObserver()\n" );
#endif
    if( observer ) {
        observer->AddRef();
        m_observerlist.push_back( observer );
        observer->OnLoad();
    }
    return NS_OK;
}

void AlarmTimerCallback(nsITimer *aTimer, void *aClosure)
{
#ifdef ICAL_DEBUG
    printf( "AlarmTimerCallback\n" );
#endif
    oeICalImpl *icallib = (oeICalImpl *)aClosure;
    icallib->SetupAlarmManager();
}

void oeICalImpl::SetupAlarmManager() {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SetupAlarmManager()\n" );
#endif

    if( m_batchMode )
    {
        #ifdef ICAL_DEBUG
            printf( "oeICalImpl::SetupAlarmManager() - defering alarms while in batch mode\n" );
        #endif
        
        return;
    }

    PRTime todayinms = PR_Now();
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( todayinms, todayinms, usecpermsec );

    icaltimetype now = ConvertFromPrtime( todayinms );

    icaltimetype nextalarm = icaltime_null_time();
    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        oeICalEventImpl *event = (oeICalEventImpl *)(tmplistptr->event);
        if( event ) {
            icaltimetype begin=icaltime_null_time();
            begin.year = 1970; begin.month=1; begin.day=1;
            icaltimetype alarmtime = begin;
            do {
                alarmtime = event->GetNextAlarmTime( alarmtime );
                if( icaltime_is_null_time( alarmtime ) )
                    break;
                if( icaltime_compare( alarmtime, now ) <= 0 ) {
#ifdef ICAL_DEBUG
                    printf( "ALARM WENT OFF: %s\n", icaltime_as_ctime( alarmtime ) );
#endif
                    
                    for( int i=0; i<m_observerlist.size(); i++ ) {
                        m_observerlist[i]->OnAlarm( event );
                    }
                }
                else {
                    if( icaltime_is_null_time( nextalarm ) )
                        nextalarm = alarmtime;
                    else if( icaltime_compare( nextalarm, alarmtime ) > 0 )
                        nextalarm = alarmtime;
                    break;
                }
            } while ( 1 );
        }
        tmplistptr = tmplistptr->next;
    }
    if( m_alarmtimer  ) {
        if ( m_alarmtimer->GetDelay() != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }
    if( !icaltime_is_null_time( nextalarm ) ) {
#ifdef ICAL_DEBUG
        printf( "NEXT ALARM IS: %s\n", icaltime_as_ctime( nextalarm ) );
#endif
        time_t timediff = icaltime_as_timet( nextalarm ) - icaltime_as_timet( now );
        
        nsresult rv;
        nsCOMPtr<nsITimer> alarmtimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
        alarmtimer->QueryInterface(NS_GET_IID(nsITimer), (void **)&m_alarmtimer);
        if( NS_FAILED( rv ) )
            m_alarmtimer = nsnull;
        else
            m_alarmtimer->Init( AlarmTimerCallback, this, timediff*1000, NS_PRIORITY_NORMAL, NS_TYPE_ONE_SHOT );
    }
}
