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
 * Cristian Tapus <cristian_tapus@yahoo.com>
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
 
 /* This file implements an XPCOM object which represents a calendar object. It is a container
 of events and prepares responses for queries made upon the calendar in whole. The task of reading from
 and writing to the calendar data storage is currently performed by this object */

#ifndef WIN32
#include <unistd.h>
#endif

#define __cplusplus__ 1

#include "oeICalImpl.h"
#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "stdlib.h"   
#include "nsCOMPtr.h"
#include "nsISimpleEnumerator.h"
#include "nsString.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsEscape.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsComponentManagerUtils.h"
#include "nsIWindowMediator.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"


extern "C" {
    #include "icalss.h"
}

icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );
void AlarmTimerCallback(nsITimer *aTimer, void *aClosure);
void GenerateUUID(char *output);

icalcomponent* icalcomponent_fetch( icalcomponent* parent,const char* uid )
{
    icalcomponent *inner;
	icalproperty *prop;
    const char *this_uid;

	for(inner = icalcomponent_get_first_component( parent , ICAL_ANY_COMPONENT );
		inner != 0;
		inner = icalcomponent_get_next_component( parent , ICAL_ANY_COMPONENT ) ){

		prop = icalcomponent_get_first_property(inner,ICAL_UID_PROPERTY);
    	if ( prop ) {
			this_uid = icalproperty_get_uid( prop );

			if(this_uid==0){
				icalerror_warn("icalcomponent_fetch found a component with no UID");
				continue;
			}
			if (strcmp(uid,this_uid)==0){
				return inner;
			}
		} else {
            icalerror_warn("icalcomponent_fetch found a component with no UID");
        }
    }
    return nsnull;
}

/*
 * This function adds the first component of the child component to the
 * first component of the parent icalfileset (which is a icalfileset_impl)
 *
 * This is used in the following member functions: AddEvent, ModifyEvent,
 * AddTodo, ModifyTodo
 *
 */
icalerrorenum icalfileset_add_first_to_first_component(icalfileset *parent,
                                       icalcomponent* child)
{

#ifdef ICAL_DEBUG
    printf("icalfileset_add_first_to_first_component()::\n");
#endif

    struct icalfileset_impl* impl = (struct icalfileset_impl*)parent;

    icalerror_check_arg_re((parent!=0),"parent", ICAL_BADARG_ERROR);
    icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

    // get the first component of the child which is either a vevent or a vtodo 
    icalcomponent* vfirst = icalcomponent_get_first_component(child, ICAL_VEVENT_COMPONENT);
    if (vfirst==0) {
      vfirst = icalcomponent_get_first_component(child, ICAL_VTODO_COMPONENT);
    }
    if (vfirst==0) return ICAL_INTERNAL_ERROR;
        
    // extract the first (presumably) vcalendar component of the parent stream 
    struct icalfileset_impl* firstc = (struct icalfileset_impl*)icalfileset_get_first_component(impl);
    
    icalcomponent_kind kind_firstc;

    if (firstc!=0) 
      kind_firstc = icalcomponent_isa (firstc);
    else 
      // the check below will fail due to firstc==0
      kind_firstc = ICAL_VCALENDAR_COMPONENT;

    if ( (firstc==0) || (kind_firstc != ICAL_VCALENDAR_COMPONENT) ) {
      // if parent stream is empty or first component is not a VCALENDAR
      // then create a new vcalendar component 
      firstc = (struct icalfileset_impl*)icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
      
      if (firstc==0) {
#ifdef ICAL_DEBUG
	printf("icalfileset_add_first_to_first_component():: firstc is still NULL\n");
#endif
	return ICAL_INTERNAL_ERROR;
      }
   
      // add component to parent stream
      icalfileset_add_component(parent, firstc);
    } else {
      // I need to remove the version and the prodid
            
      //version
      icalproperty *prop = icalcomponent_get_first_property(firstc, ICAL_VERSION_PROPERTY );
      icalcomponent_remove_property( firstc, prop );
      
      //prodid
      prop = icalcomponent_get_first_property( firstc, ICAL_PRODID_PROPERTY );
      icalcomponent_remove_property( firstc, prop );
    }
    
      
    //version
    icalproperty *prop = icalproperty_new_version( ICALEVENT_VERSION );
    icalcomponent_add_property( firstc, prop );
    
    //prodid
    prop = icalproperty_new_prodid( ICALEVENT_PRODID );
    icalcomponent_add_property( firstc, prop );

    // get the timezone component if any 
    icalcomponent* timezone = icalcomponent_get_first_component(child, ICAL_VTIMEZONE_COMPONENT);

    // we know that we have at most one timezone component inside the child component
    if (timezone!=0) icalcomponent_add_component(firstc, timezone);
    
    // add first component of child into the first component of stream
    icalcomponent_add_component(firstc, vfirst);
    
    icalfileset_mark(parent);
    
    return ICAL_NO_ERROR;

}


oeEventEnumerator::oeEventEnumerator( ) 
:
    mCurrentIndex( 0 )
{
    NS_NewISupportsArray(getter_AddRefs(mEventVector));
}

oeEventEnumerator::~oeEventEnumerator()
{
}

NS_IMPL_ISUPPORTS1(oeEventEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeEventEnumerator::HasMoreElements(PRBool *result)
{
    PRUint32 count = 0;
    mEventVector->Count(&count);
    if( mCurrentIndex >= count )
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
        
    PRUint32 count = 0;
    mEventVector->Count(&count);
    if( mCurrentIndex >= count )
    {
        *_retval = nsnull;
    }
    else
    {
        nsCOMPtr<nsISupports> event;
        mEventVector->QueryElementAt( mCurrentIndex, NS_GET_IID(nsISupports), getter_AddRefs(event));
        *_retval = event;
        NS_ADDREF( *_retval );
        ++mCurrentIndex;
    }
    
    return NS_OK;
}


NS_IMETHODIMP
oeEventEnumerator::AddEvent( nsISupports* event)
{
    PRUint32 count = 0;
    mEventVector->Count( &count );
    mEventVector->InsertElementAt( event, count );
    return NS_OK;
}

/* date enumerator */
oeDateEnumerator::oeDateEnumerator( ) 
:
    mCurrentIndex( 0 )
{
}

oeDateEnumerator::~oeDateEnumerator()
{
    for( int i=0; i<mDateVector.Count(); i++ ) {
        PRTime *timeptr = (PRTime*)(mDateVector[i]);
        delete timeptr;
    }
    mDateVector.Clear();
}

NS_IMPL_ISUPPORTS1(oeDateEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
oeDateEnumerator::HasMoreElements(PRBool *result)
{
    if( mCurrentIndex >= mDateVector.Count() )
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

    if( mCurrentIndex >= mDateVector.Count() ) 
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

        date->SetData( *(PRTime *)(mDateVector[ mCurrentIndex ]) ); 
        *_retval = date; 
        ++mCurrentIndex; 
    } 

    return NS_OK; 
} 

NS_IMETHODIMP
oeDateEnumerator::AddDate(PRTime date)
{
    PRTime *newdate = new PRTime;
    *newdate = date;
    mDateVector.AppendElement( newdate );
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

        m_batchMode = false;

        m_alarmtimer = nsnull;

        m_filter = new oeICalFilter();
        NS_ADDREF( m_filter );

        NS_NewISupportsArray(getter_AddRefs(m_observerlist));
        NS_NewISupportsArray(getter_AddRefs(m_todoobserverlist));
}

oeICalImpl::~oeICalImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::~oeICalImpl()\n" );
#endif
    if( m_alarmtimer  ) {
        PRUint32 delay = 0;
        #ifdef NS_INIT_REFCNT //A temporary way of keeping backward compatibility with Mozilla 1.0 source compile
        delay = m_alarmtimer->GetDelay();
        #else
        m_alarmtimer->GetDelay( &delay );
        #endif
        if ( delay != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }

    NS_RELEASE( m_filter );
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
NS_IMPL_ISUPPORTS1(oeICalImpl, oeIICal)

EventList *oeICalImpl::GetEventList()
{
    return &m_eventlist;
}

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
    icalset *stream;
    icalcomponent *icalcalendar,*icalevent;
    struct icaltimetype start, end;
    char uidstr[10]="900000000";
    char icalrawcalendarstr[] = "BEGIN:VCALENDAR\n\
BEGIN:VEVENT\n\
END:VEVENT\n\
END:VCALENDAR\n\
";
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_ERROR_FAILURE;
    }

    icalcalendar = icalparser_parse_string(icalrawcalendarstr);
    if ( !icalcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot create VCALENDAR!\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    
    icalevent = icalcomponent_get_first_component(icalcalendar,ICAL_VEVENT_COMPONENT);
    if ( !icalevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_ERROR_FAILURE;
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

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::Test() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
#ifdef ICAL_DEBUG
	printf("Appended Event\n");
#endif
    
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    if ( !fetchedcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot fetch event!\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
	
    icalcomponent *fetchedevent = icalcomponent_get_first_component( fetchedcal,ICAL_VEVENT_COMPONENT);
    if ( !fetchedevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
    }
    end = icalproperty_get_dtstart( tmpprop );
#ifdef ICAL_DEBUG
    printf("End: %d-%d-%d %d:%d\n", end.year, end.month, end.day, end.hour, end.minute );
#endif
    //
    for( tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_X_PROPERTY );
            tmpprop != 0 ;
            tmpprop = icalcomponent_get_next_property( fetchedevent, ICAL_X_PROPERTY ) ) {
#ifdef ICAL_DEBUG
            icalparameter *tmppar = icalproperty_get_first_parameter( tmpprop, ICAL_MEMBER_PARAMETER );
            printf("%s: %s\n", icalparameter_get_member( tmppar ), icalproperty_get_value_as_string( tmpprop ) );
#endif
    }

    icalcomponent *newcomp = icalcomponent_new_clone( fetchedcal );
    if ( !newcomp ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: Cannot clone event!\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    
    icalcomponent *newevent = icalcomponent_get_first_component( newcomp, ICAL_VEVENT_COMPONENT );
    if ( !newevent ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: VEVENT not found!\n" );
        #endif
        return NS_ERROR_FAILURE;
    }
    tmpprop = icalcomponent_get_first_property( newevent, ICAL_SUMMARY_PROPERTY );
    if ( !tmpprop ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::Test() failed: SUMMARY not found!\n" );
        #endif
        return NS_ERROR_FAILURE;
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
        return NS_ERROR_FAILURE;
    }
    icalfileset_remove_component( stream, fetchedcal );
	
#ifdef ICAL_DEBUG
    printf("Removed Event\n");
#endif

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::Test() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::SetServer( const char *str ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::SetServer(%s)\n", str );
#endif

    if( strncmp( str, "file:///", strlen( "file:///" ) ) == 0 ) {
        nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
        nsCString filePath;
        filePath = str;
        url->SetSpec( filePath );
        url->GetFilePath( filePath );
        strcpy( serveraddr, filePath.get() );
        NS_UnescapeURL( serveraddr );
    } else
	    strcpy( serveraddr, str );

    icalset *stream;
    
    //UGLY HACK TO ADD DEPENDANCY TO stat() in libc. ( -lc didn't work )
    int dummy=0;
    if( dummy )
        stat(NULL,NULL);

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::SetServer() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    nsresult rv;
    icalcomponent *vcalendar;
    icalcomponent *vevent,*vtodo;
    oeICalEventImpl *icalevent;
    oeICalTodoImpl *icaltodo;
    for( vcalendar = icalfileset_get_first_component( stream );
        vcalendar != 0;
        vcalendar = icalfileset_get_next_component( stream ) ) {
        
        for( vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
            vevent != 0;
            vevent = icalcomponent_get_next_component( vcalendar, ICAL_VEVENT_COMPONENT ) ) {

            if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &icalevent ))) {
                return rv;
            }
            if( icalevent->ParseIcalComponent( vevent ) ) {
                m_eventlist.Add( icalevent );
                icalevent->SetParent( this );
            } else {
                icalevent->Release();
            }
        }
        for( vtodo = icalcomponent_get_first_component( vcalendar, ICAL_VTODO_COMPONENT );
            vtodo != 0;
            vtodo = icalcomponent_get_next_component( vcalendar, ICAL_VTODO_COMPONENT ) ) {

            if( NS_FAILED( rv = NS_NewICalTodo((oeIICalTodo**) &icaltodo ))) {
                return rv;
            }
            if( icaltodo->ParseIcalComponent( vtodo ) ) {
                m_todolist.Add( icaltodo );
                icaltodo->SetParent( this );
            } else {
                icaltodo->Release();
            }
        }
    }
    
    icalfileset_free(stream);
    
    unsigned int i;
    PRUint32 observercount;
    m_observerlist->Count(&observercount);
    for( i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalObserver>observer;
        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnLoad();
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::SetServer() : WARNING Call to observer's onLoad() unsuccessful: %x\n", rv );
        }
        #endif
    }
    m_todoobserverlist->Count(&observercount);
    for( i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalTodoObserver>observer;
        m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnLoad();
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::SetServer() : WARNING Call to observer's onLoad() unsuccessful: %x\n", rv );
        }
        #endif
    }
    
    SetupAlarmManager();

    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetServer( char **server ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetServer()\n" );
#endif
    *server= (char*) nsMemory::Clone( serveraddr, strlen(serveraddr)+1);
    if( *server == nsnull )
        return  NS_ERROR_OUT_OF_MEMORY;
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

//The container will take care of this for now
/*        
        unsigned int i;
        for( i=0; i<m_observerlist.size(); i++ ) 
        {
            if( m_batchMode ) {
                nsresult rv;
                rv = m_observerlist[i]->OnStartBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalImpl::SetBatchMode() : WARNING Call to observer's onStartBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }
            else {
                nsresult rv;
                rv = m_observerlist[i]->OnEndBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalImpl::SetBatchMode() : WARNING Call to observer's onEndBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }

        }
        for( i=0; i<m_todoobserverlist.size(); i++ ) 
        {
            if( m_batchMode ) {
                nsresult rv;
                rv = m_todoobserverlist[i]->OnStartBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalImpl::SetBatchMode() : WARNING Call to observer's onStartBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }
            else {
                nsresult rv;
                rv = m_todoobserverlist[i]->OnEndBatch();
                #ifdef ICAL_DEBUG
                if( NS_FAILED( rv ) ) {
                    printf( "oeICalImpl::SetBatchMode() : WARNING Call to observer's onEndBatch() unsuccessful: %x\n", rv );
                }
                #endif
            }

        }*/
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
NS_IMETHODIMP oeICalImpl::AddEvent(oeIICalEvent *icalevent,char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddEvent()\n" );
#endif
    icalset *stream;
    icalcomponent *vcalendar;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::AddEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    ((oeICalEventImpl *)icalevent)->GetId( retid );

    if( *retid == nsnull ) {
        char uidstr[40];
        GenerateUUID( uidstr );
        ((oeICalEventImpl *)icalevent)->SetId( uidstr );
        ((oeICalEventImpl *)icalevent)->GetId( retid );
    }

    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
    icalfileset_add_first_to_first_component( stream, vcalendar );

    if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::AddEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free( stream );

    icalevent->AddRef();
    m_eventlist.Add( icalevent );
    ((oeICalEventImpl *)icalevent)->SetParent( this );

    PRUint32 observercount;
    m_observerlist->Count(&observercount);
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalObserver>observer;
        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnAddItem( icalevent );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::AddEvent() : WARNING Call to observer's onAddItem() unsuccessful: %x\n", rv );
        }
        #endif
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
NS_IMETHODIMP oeICalImpl::ModifyEvent(oeIICalEvent *icalevent, char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::ModifyEvent()\n" );
#endif
    icalset *stream;
    icalcomponent *vcalendar;
    nsresult rv;

    //This might be a TODO object. If so then call the appropriate function.
    nsCOMPtr<oeIICalTodo> icaltodo;
    rv = icalevent->QueryInterface(NS_GET_IID(oeIICalTodo), (void **)&icaltodo);
    if( NS_SUCCEEDED( rv ) ) {
        return ModifyTodo( icaltodo, retid );
    }

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    icalevent->GetId( retid );
    if( *retid == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }
    
    icalcomponent *fetchedvcal = icalfileset_fetch( stream, *retid );
    
    oeICalEventImpl *oldevent=nsnull;
    if( fetchedvcal ) {
        icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, *retid );
        if( fetchedvevent ) {
            icalcomponent_remove_component( fetchedvcal, fetchedvevent );
            if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
                icalfileset_remove_component( stream, fetchedvcal );
                icalcomponent_free( fetchedvcal );
            }
            if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &oldevent ))) {
                nsMemory::Free( *retid );
                *retid = nsnull;
                icalfileset_free(stream);
                return rv;
            }
            oldevent->ParseIcalComponent( fetchedvevent );
            icalcomponent_free( fetchedvevent );
        } else {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::ModifyEvent() - WARNING Event not found.\n" );
            #endif
            nsMemory::Free( *retid );
            *retid = nsnull;
            icalfileset_free(stream);
            return NS_OK;
        }
    } else {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyEvent() - WARNING Event not found.\n" );
        #endif
        nsMemory::Free( *retid );
        *retid = nsnull;
        icalfileset_free(stream);
        return NS_OK;
    }
    
    //Update Last-Modified
    icalevent->UpdateLastModified();

    vcalendar = ((oeICalEventImpl *)icalevent)->AsIcalComponent();
    icalfileset_add_first_to_first_component( stream, vcalendar );  
  
    if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::ModifyEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
    
    PRUint32 observercount;
    m_observerlist->Count(&observercount);
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalObserver>observer;
        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
        rv = observer->OnModifyItem( icalevent, oldevent );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::ModifyEvent() : WARNING Call to observer's onModifyItem() unsuccessful: %x\n", rv );
        }
        #endif
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
NS_IMETHODIMP oeICalImpl::FetchEvent( const char *id, oeIICalEvent **ev)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::FetchEvent()\n" );
#endif

    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::FetchEvent() - Invalid Id.\n" );
        #endif
        *ev = nsnull;
        return NS_OK;
    }

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
oeICalImpl::DeleteEvent( const char *id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::DeleteEvent( %s )\n", id );
#endif
    icalset *stream;
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }

    icalcomponent *fetchedvcal = icalfileset_fetch( stream, id );
    
    if( !fetchedvcal ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }
    
    icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, id );
    
    if( !fetchedvevent ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteEvent() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }

    icalcomponent_remove_component( fetchedvcal, fetchedvevent );
    icalcomponent_free( fetchedvevent );
    if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
        icalfileset_remove_component( stream, fetchedvcal );
        icalcomponent_free( fetchedvcal );
    }
	
    icalfileset_mark( stream ); //Make sure stream is marked as dirty
	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::DeleteEvent() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
	
    oeIICalEvent *icalevent;
    FetchEvent( id , &icalevent );

    m_eventlist.Remove( id );
    
    PRUint32 observercount;
    m_observerlist->Count(&observercount);
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalObserver>observer;
        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnDeleteItem( icalevent );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::DeleteEvent() : WARNING Call to observer's onDeleteItem() unsuccessful: %x\n", rv );
        }
        #endif
    }

    icalevent->Release();

    SetupAlarmManager();
	return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetAllEvents(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetAllEvents()\n" );
#endif

    nsCOMPtr<oeEventEnumerator> eventEnum;
    if( !*resultList ) {
        eventEnum = new oeEventEnumerator();
        if (!eventEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
    } else
        eventEnum = (oeEventEnumerator *)*resultList;

    nsCOMPtr<nsISupportsArray> eventArray;
    NS_NewISupportsArray(getter_AddRefs(eventArray));
    if (eventArray == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            eventArray->AppendElement( tmplistptr->event );
        }
        tmplistptr = tmplistptr->next;
    }

    PRTime todayinms = PR_Now();
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( todayinms, todayinms, usecpermsec );

    struct icaltimetype checkdate = ConvertFromPrtime( todayinms );
    struct icaltimetype now = ConvertFromPrtime( todayinms );
    icaltime_adjust( &now, 0, 0, 0, -1 );

    icaltimetype nextcheckdate;
    PRUint32 num;

    do {
        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            nsCOMPtr<oeIICalEvent> tmpcomp;
            eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
            oeIICalEvent* tmpevent = tmpcomp;
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( now, nsnull );
            if( !icaltime_is_null_time( next ) )
                continue;
            icaltimetype previous = ((oeICalEventImpl *)tmpevent)->GetPreviousOccurrence( checkdate );
            if( !icaltime_is_null_time( previous ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, previous ) > 0) ) ) {
                soonest = previous;
            }
        }

        nextcheckdate = soonest;
        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                nsCOMPtr<oeIICalEvent> tmpcomp;
                eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
                oeIICalEvent* tmpevent = tmpcomp;
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( now, nsnull  );
                if( !icaltime_is_null_time( next ) )
                    continue;
                icaltimetype previous = ((oeICalEventImpl *)tmpevent)->GetPreviousOccurrence( checkdate );
                if( !icaltime_is_null_time( previous ) && (icaltime_compare( nextcheckdate, previous ) == 0) ) {
                    eventEnum->AddEvent( tmpevent );
//                    PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
//                    dateEnum->AddDate( nextdateinms );
                    eventArray->RemoveElementAt( i );
                    break;
                }
            }
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    checkdate = ConvertFromPrtime( todayinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    do {

        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            nsCOMPtr<oeIICalEvent> tmpcomp;
            eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
            oeIICalEvent* tmpevent = tmpcomp;
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, nsnull  );
            next.is_date = false;
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }

        nextcheckdate = soonest;

        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                nsCOMPtr<oeIICalEvent> tmpcomp;
                eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
                oeIICalEvent* tmpevent = tmpcomp;
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, nsnull  );
                next.is_date = false;
                if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                    eventEnum->AddEvent( tmpevent );
//                    PRTime nextdateinms = ConvertToPrtime( nextcheckdate );
//                    dateEnum->AddDate( nextdateinms );
                    eventArray->RemoveElementAt( i );
                    icaltime_adjust( &nextcheckdate, 0, 0, 0, -1 );
                    break;
                }
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    // bump ref count
//    return eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetEventsForMonth( PRTime datems, nsISimpleEnumerator **eventlist ) {
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

    return GetEventsForRange(checkdateinms ,checkenddateinms, eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForWeek( PRTime datems, nsISimpleEnumerator **eventlist ) {
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

    return GetEventsForRange(checkdateinms ,checkenddateinms, eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForDay( PRTime datems, nsISimpleEnumerator **eventlist ) {
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

    return GetEventsForRange(checkdateinms ,checkenddateinms, eventlist );
}

NS_IMETHODIMP
oeICalImpl::GetEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetEventsForRange()\n" );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum;
    if( !*eventlist ) {
        eventEnum = new oeEventEnumerator();
        if (!eventEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    } else
        eventEnum = (oeEventEnumerator *)*eventlist;

    struct icaltimetype checkdate = ConvertFromPrtime( checkdateinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    struct icaltimetype checkenddate = ConvertFromPrtime( checkenddateinms );

    icaltimetype nextcheckdate;
    do {
        nextcheckdate = GetNextEvent( checkdate );
        if( icaltime_compare( nextcheckdate, checkenddate ) >= 0 )
            break;
        if( !icaltime_is_null_time( nextcheckdate )) {
            EventList *tmplistptr = &m_eventlist;
            while( tmplistptr ) {
                if( tmplistptr->event ) {
                    oeIICalEvent* tmpevent = tmplistptr->event;
                    bool isbeginning;
                    icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, &isbeginning );
                    bool isallday = next.is_date;
                    next.is_date = false;
                    if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                        ((oeICalEventImpl *)tmpevent)->ChopAndAddEventToEnum( nextcheckdate, eventlist, isallday, isbeginning );
                    }
                }
                tmplistptr = tmplistptr->next;
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetFirstEventsForRange( PRTime checkdateinms, PRTime checkenddateinms, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetFirstEventsForRange()\n" );
#endif
    nsCOMPtr<oeEventEnumerator> eventEnum;
    if( !*eventlist ) {
        eventEnum = new oeEventEnumerator();
        if (!eventEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    } else
        eventEnum = (oeEventEnumerator *)*eventlist;

    nsCOMPtr<nsISupportsArray> eventArray;
    NS_NewISupportsArray(getter_AddRefs(eventArray));
    if (eventArray == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            eventArray->AppendElement( tmplistptr->event );
        }
        tmplistptr = tmplistptr->next;
    }

    struct icaltimetype checkdate = ConvertFromPrtime( checkdateinms );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );

    struct icaltimetype checkenddate = ConvertFromPrtime( checkenddateinms );

    icaltimetype nextcheckdate;
    do {
        PRUint32 num;
        icaltimetype soonest = icaltime_null_time();
        eventArray->Count( &num );
        for ( unsigned int i=0; i<num; i++ ) {
            nsCOMPtr<oeIICalEvent> tmpcomp;
            eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
            oeIICalEvent* tmpevent = tmpcomp;
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, nsnull );
            next.is_date = false;
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }

        if ( icaltime_compare( soonest, checkenddate ) > 0 ) {
            nextcheckdate = icaltime_null_time();
        } else 
            nextcheckdate = soonest;

        if( !icaltime_is_null_time( nextcheckdate )) {

            for ( unsigned int i=0; i<num; i++ ) {
                nsCOMPtr<oeIICalEvent> tmpcomp;
                eventArray->GetElementAt( i, getter_AddRefs( tmpcomp ) );
                oeIICalEvent* tmpevent = tmpcomp;
                bool isbeginning;
                icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( checkdate, &isbeginning );
                bool isallday = next.is_date;
                next.is_date = false;
                if( !icaltime_is_null_time( next ) && (icaltime_compare( nextcheckdate, next ) == 0) ) {
                    ((oeICalEventImpl *)tmpevent)->ChopAndAddEventToEnum( nextcheckdate, eventlist, isallday, isbeginning );
                    eventArray->RemoveElementAt( i );
                    icaltime_adjust( &nextcheckdate, 0, 0, 0, -1 );
                    break;
                }
            }
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) );

    eventArray->Clear();
    return NS_OK;
}

icaltimetype oeICalImpl::GetNextEvent( icaltimetype starting ) {
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::GetNextEvent()\n" );
#endif
    icaltimetype soonest = icaltime_null_time();
    
    EventList *tmplistptr = &m_eventlist;
    while( tmplistptr ) {
        if( tmplistptr->event ) {
            oeIICalEvent* tmpevent = tmplistptr->event;
            icaltimetype next = ((oeICalEventImpl *)tmpevent)->GetNextRecurrence( starting, nsnull );
            next.is_date = false;
            if( !icaltime_is_null_time( next ) && ( icaltime_is_null_time( soonest ) || (icaltime_compare( soonest, next ) > 0) ) ) {
                soonest = next;
            }
        }
        tmplistptr = tmplistptr->next;
    }
    return soonest;
}

NS_IMETHODIMP
oeICalImpl::GetNextNEvents( PRTime datems, PRInt32 maxcount, nsISimpleEnumerator **eventlist ) {
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetNextNEvents( %d )\n", maxcount );
#endif
    
    nsCOMPtr<oeEventEnumerator> eventEnum;
    if( !*eventlist ) {
        eventEnum = new oeEventEnumerator();
        if (!eventEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        eventEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)eventlist);
    } else
        eventEnum = (oeEventEnumerator *)*eventlist;

    struct icaltimetype checkdate = ConvertFromPrtime( datems );
    icaltime_adjust( &checkdate, 0, 0, 0, -1 );
    
    int count = 0;
    icaltimetype nextcheckdate;
    do {
        nextcheckdate = GetNextEvent( checkdate );
        if( !icaltime_is_null_time( nextcheckdate )) {
            EventList *tmplistptr = &m_eventlist;
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
            checkdate = nextcheckdate;
        }
    } while ( !icaltime_is_null_time( nextcheckdate ) && (count < maxcount) );

    return NS_OK;
}

#ifdef ICAL_DEBUG_ALL
int gObservercount=0;
#endif

NS_IMETHODIMP 
oeICalImpl::AddObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    gObservercount++;
    printf( "oeICalImpl::AddObserver(): %d\n", gObservercount );
#endif
    if( observer ) {
        observer->AddRef();
        PRUint32 observercount;
        m_observerlist->Count( &observercount );
        m_observerlist->InsertElementAt( observer, observercount );
//        observer->OnLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::RemoveObserver(oeIICalObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    gObservercount--;
    printf( "oeICalImpl::RemoveObserver(): %d\n", gObservercount );
#endif
    if( observer ) {
        PRUint32 observercount;
        m_observerlist->Count( &observercount );
        for( unsigned int i=0; i<observercount; i++ ) {
            nsCOMPtr<oeIICalObserver>tmpobserver;
            m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(tmpobserver));
            if( observer == tmpobserver ) {
                m_observerlist->RemoveElementAt( i );
                observer->Release();
                break;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::AddTodoObserver(oeIICalTodoObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::AddTodoObserver()\n" );
#endif
    if( observer ) {
        observer->AddRef();
        PRUint32 observercount;
        m_todoobserverlist->Count( &observercount );
        m_todoobserverlist->InsertElementAt( observer, observercount );
//        observer->OnLoad();
    }
    return NS_OK;
}

NS_IMETHODIMP 
oeICalImpl::RemoveTodoObserver(oeIICalTodoObserver *observer)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::RemoveTodoObserver()\n" );
#endif
    if( observer ) {
        PRUint32 observercount;
        m_todoobserverlist->Count( &observercount );
        for( unsigned int i=0; i<observercount; i++ ) {
            nsCOMPtr<oeIICalTodoObserver>tmpobserver;
            m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(tmpobserver));
            if( observer == tmpobserver ) {
                m_todoobserverlist->RemoveElementAt( i );
                observer->Release();
                break;
            }
        }
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

NS_IMETHODIMP UpdateCalendarIcon( PRBool hasAlarm )
{
    nsresult rv;
    nsCOMPtr<nsIWindowMediator> windowMediator =
    do_GetService(NS_WINDOWMEDIATOR_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    static PRBool lastTimeHadAlarm = false;
 
    if( lastTimeHadAlarm == hasAlarm )
        return NS_OK;

    lastTimeHadAlarm = hasAlarm;
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
 
    // why use DOM window enumerator instead of XUL window...????
    if (NS_SUCCEEDED(windowMediator->GetEnumerator(nsnull, getter_AddRefs(windowEnumerator))))
    {
        PRBool more;
   
        windowEnumerator->HasMoreElements(&more);
   
        while(more)
        {
            nsCOMPtr<nsISupports> nextWindow = nsnull;
            windowEnumerator->GetNext(getter_AddRefs(nextWindow));
            nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(nextWindow));

            if( domWindow ) {
                nsCOMPtr<nsIDOMDocument> domDocument;
                domWindow->GetDocument(getter_AddRefs(domDocument));

                if(domDocument)
                {
                    nsCOMPtr<nsIDOMElement> domElement;
                    domDocument->GetElementById(NS_LITERAL_STRING("mini-cal"), getter_AddRefs(domElement));

                    if (domElement) {
                        if ( hasAlarm ) {
                            domElement->SetAttribute(NS_LITERAL_STRING("BiffState"), NS_LITERAL_STRING("Alarm"));
                        }
                        else {
                            domElement->RemoveAttribute(NS_LITERAL_STRING("BiffState"));
                        }
                    }
                }
            }

            windowEnumerator->HasMoreElements(&more);
        }
    }
    return NS_OK;
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

    static icaltimetype lastcheck = icaltime_null_time();

    UpdateCalendarIcon( false );

    PRTime todayinms = PR_Now();
    PRInt64 usecpermsec;
    LL_I2L( usecpermsec, PR_USEC_PER_MSEC );
    LL_DIV( todayinms, todayinms, usecpermsec );

    icaltimetype now = ConvertFromPrtime( todayinms );

    icaltimetype nextalarm = icaltime_null_time();
    EventList *tmplistptr = &m_eventlist;

    int processmissed = -1; // -1 means the value has not been read from the preferences. 0 or 1 are valid values.

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
                    printf( "ALARM WENT OFF: %s\n", icaltime_as_ical_string( alarmtime ) );
                    #endif
                    
                    nsresult rv;
                    if( processmissed == -1 ) {
                        nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
                        if ( NS_SUCCEEDED(rv) && prefBranch ) {
                            rv = prefBranch->GetBoolPref("calendar.alarms.showmissed", &processmissed);
                        } else {
                            processmissed = true; //if anything goes wrong just consider the default setting
                        }
                    }

                    if( !processmissed ) {
                        time_t timediff = icaltime_as_timet( now ) - icaltime_as_timet( alarmtime );
                        if( timediff > 30 ) //if alarmtime is older than 30 seconds it won't be processed.
                            continue;
                    }

                    UpdateCalendarIcon( true );

                    oeIICalEventDisplay* eventDisplay;
                    rv = NS_NewICalEventDisplay( event, &eventDisplay );
                    #ifdef ICAL_DEBUG
                    if( NS_FAILED( rv ) ) {
                        printf( "oeICalImpl::SetupAlarmManager() : WARNING Cannot create oeIICalEventDisplay instance: %x\n", rv );
                    }
                    #endif
                    icaltimetype eventtime = event->CalculateEventTime( alarmtime );
                    eventDisplay->SetDisplayDate( ConvertToPrtime( eventtime ) );
                    PRUint32 observercount;
                    m_observerlist->Count( &observercount );
                    for( unsigned int i=0; i<observercount; i++ ) {
                        nsCOMPtr<oeIICalObserver>observer;
                        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
                        rv = observer->OnAlarm( eventDisplay );
                        #ifdef ICAL_DEBUG
                        if( NS_FAILED( rv ) ) {
                            printf( "oeICalImpl::SetupAlarmManager() : WARNING Call to observer's onAlarm() unsuccessful: %x\n", rv );
                        }
                        #endif
                    }
                    NS_RELEASE( eventDisplay );
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

    TodoList *tmptodolistptr = &m_todolist;
    while( tmptodolistptr ) {
        oeIICalTodo *todo = tmptodolistptr->todo;
        if( todo ) {
            oeICalEventImpl *event = ((oeICalTodoImpl *)todo)->GetBaseEvent();
            icaltimetype begin=icaltime_null_time();
            begin.year = 1970; begin.month=1; begin.day=1;
            icaltimetype alarmtime = begin;
            do {
                alarmtime = event->GetNextAlarmTime( alarmtime );
                if( icaltime_is_null_time( alarmtime ) )
                    break;
                if( icaltime_compare( alarmtime, now ) <= 0 ) {
                    #ifdef ICAL_DEBUG
                    printf( "ALARM WENT OFF: %s\n", icaltime_as_ical_string( alarmtime ) );
                    #endif
                    
                    nsresult rv;
                    if( processmissed == -1 ) {
                        nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
                        if ( NS_SUCCEEDED(rv) && prefBranch ) {
                            rv = prefBranch->GetBoolPref("calendar.alarms.showmissed", &processmissed);
                        } else {
                            processmissed = true; //if anything goes wrong just consider the default setting
                        }
                    }

                    if( !processmissed ) {
                        time_t timediff = icaltime_as_timet( now ) - icaltime_as_timet( alarmtime );
                        if( timediff > 30 ) //if alarmtime is older than 30 seconds it won't be processed.
                            continue;
                    }

                    UpdateCalendarIcon( true );

                    oeIICalEventDisplay* eventDisplay;
                    rv = NS_NewICalEventDisplay( todo, &eventDisplay );
                    #ifdef ICAL_DEBUG
                    if( NS_FAILED( rv ) ) {
                        printf( "oeICalImpl::SetupAlarmManager() : WARNING Cannot create oeIICalEventDisplay instance: %x\n", rv );
                    }
                    #endif
                    icaltimetype eventtime = event->CalculateEventTime( alarmtime );
                    eventDisplay->SetDisplayDate( ConvertToPrtime( eventtime ) );
                    PRUint32 observercount;
                    //Here we should be using the todo observer list but nothing implements 
                    //alarm handling for todos yet so we'll just use the one for events
                    m_observerlist->Count( &observercount ); 
                    for( unsigned int i=0; i<observercount; i++ ) {
                        nsCOMPtr<oeIICalObserver>observer;
                        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
                        rv = observer->OnAlarm( eventDisplay );
                        #ifdef ICAL_DEBUG
                        if( NS_FAILED( rv ) ) {
                            printf( "oeICalImpl::SetupAlarmManager() : WARNING Call to observer's onAlarm() unsuccessful: %x\n", rv );
                        }
                        #endif
                    }
                    NS_RELEASE( eventDisplay );
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
        tmptodolistptr = tmptodolistptr->next;
    }


    lastcheck = now;

    if( m_alarmtimer  ) {
        PRUint32 delay = 0;
        #ifdef NS_INIT_REFCNT //A temporary way of keeping backward compatibility with Mozilla 1.0 source compile
        delay = m_alarmtimer->GetDelay();
        #else
        m_alarmtimer->GetDelay( &delay );
        #endif
        if ( delay != 0 )
            m_alarmtimer->Cancel();
        m_alarmtimer->Release();
        m_alarmtimer = nsnull;
    }
    if( !icaltime_is_null_time( nextalarm ) ) {
        #ifdef ICAL_DEBUG
        printf( "NEXT ALARM IS: %s\n", icaltime_as_ical_string( nextalarm ) );
        #endif
        time_t timediff = icaltime_as_timet( nextalarm ) - icaltime_as_timet( now );
        
        nsresult rv;
        nsCOMPtr<nsITimer> alarmtimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
        alarmtimer->QueryInterface(NS_GET_IID(nsITimer), (void **)&m_alarmtimer);
        if( NS_FAILED( rv ) )
            m_alarmtimer = nsnull;
        else
            #ifdef NS_INIT_REFCNT //A temporary way of keeping backward compatibility with Mozilla 1.0 source compile
            m_alarmtimer->Init( AlarmTimerCallback, this, timediff*1000, PR_TRUE, NS_TYPE_ONE_SHOT );
            #else
            m_alarmtimer->InitWithFuncCallback( AlarmTimerCallback, this, timediff*1000, nsITimer::TYPE_ONE_SHOT );
            #endif
    }
}

/**
*
*   AddTodo
*
*   DESCRIPTION: Adds a TODO component
*
*   PARAMETERS: 
*	icaltodo - The XPCOM component representing a TODO
*	retid   - Place to store the returned id
*
*/
NS_IMETHODIMP oeICalImpl::AddTodo(oeIICalTodo *icaltodo,char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::AddTodo()\n" );
#endif
    icalset *stream;
    icalcomponent *vcalendar;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::AddTodo() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    ((oeICalTodoImpl *)icaltodo)->GetId( retid );

    if( *retid == nsnull ) {
        char uidstr[40];
        GenerateUUID( uidstr );

        ((oeICalTodoImpl *)icaltodo)->SetId( uidstr );
        ((oeICalTodoImpl *)icaltodo)->GetId( retid );
    }

    vcalendar = ((oeICalTodoImpl *)icaltodo)->AsIcalComponent();
	
    icalfileset_add_first_to_first_component( stream, vcalendar );  

	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::AddTodo() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free( stream );

    icaltodo->AddRef();
    m_todolist.Add( icaltodo );
    ((oeICalTodoImpl *)icaltodo)->SetParent( this );

    PRUint32 observercount;
    m_todoobserverlist->Count( &observercount );
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalTodoObserver>observer;
        m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnAddItem( icaltodo );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::AddTodo() : WARNING Call to observer's onAddItem() unsuccessful: %x\n", rv );
        }
        #endif
    }

    SetupAlarmManager();
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::DeleteTodo( const char *id )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::DeleteTodo( %s )\n", id );
#endif
    icalset *stream;
    
    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteTodo() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteTodo() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }

    icalcomponent *fetchedvcal = icalfileset_fetch( stream, id );
    
    if( !fetchedvcal ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteTodo() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }
    icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, id );
    
    if( !fetchedvevent ) {
        icalfileset_free(stream);
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::DeleteTodo() - WARNING Event not found.\n" );
        #endif
        return NS_OK;
    }

    icalcomponent_remove_component( fetchedvcal, fetchedvevent );
    icalcomponent_free( fetchedvevent );
    if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
        icalfileset_remove_component( stream, fetchedvcal );
        icalcomponent_free( fetchedvcal );
    }
	
    icalfileset_mark( stream ); //Make sure stream is marked as dirty
	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::DeleteTodo() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
	
    oeIICalTodo *icalevent;
    FetchTodo( id , &icalevent );

    m_todolist.Remove( id );
    
    PRUint32 observercount;
    m_todoobserverlist->Count( &observercount );
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalTodoObserver>observer;
        m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnDeleteItem( icalevent );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::DeleteTodo() : WARNING Call to observer's onDeleteItem() unsuccessful: %x\n", rv );
        }
        #endif
    }

    icalevent->Release();

    SetupAlarmManager();
	return NS_OK;
}

NS_IMETHODIMP oeICalImpl::FetchTodo( const char *id, oeIICalTodo **ev)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalImpl::FetchTodo()\n" );
#endif

    if( id == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::FetchTodo() - Invalid Id.\n" );
        #endif
        *ev = nsnull;
        return NS_OK;
    }

    oeIICalTodo* event = m_todolist.GetTodoById( id );
    if( event != nsnull ) {
        event->AddRef();
    }
    *ev = event;
    return NS_OK;
}

NS_IMETHODIMP oeICalImpl::ModifyTodo(oeIICalTodo *icalevent, char **retid)
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::ModifyTodo()\n" );
#endif
    icalset *stream;
    icalcomponent *vcalendar;

    stream = icalfileset_new(serveraddr);
    if ( !stream ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyTodo() failed: Cannot open stream: %s!\n", serveraddr );
        #endif
        return NS_OK;
    }
    
    icalevent->GetId( retid );
    if( *retid == nsnull ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyTodo() - Invalid Id.\n" );
        #endif
        icalfileset_free(stream);
        return NS_OK;
    }
    
    icalcomponent *fetchedvcal = icalfileset_fetch( stream, *retid );
    
    oeICalTodoImpl *oldevent=nsnull;
    if( fetchedvcal ) {
        icalcomponent *fetchedvevent = icalcomponent_fetch( fetchedvcal, *retid );
        if( fetchedvevent ) {
            icalcomponent_remove_component( fetchedvcal, fetchedvevent );
            if ( !icalcomponent_get_first_real_component( fetchedvcal ) ) {
                icalfileset_remove_component( stream, fetchedvcal );
                icalcomponent_free( fetchedvcal );
            }
            nsresult rv;
            if( NS_FAILED( rv = NS_NewICalTodo((oeIICalTodo**) &oldevent ))) {
                nsMemory::Free( *retid );
                *retid = nsnull;
                icalfileset_free(stream);
                return rv;
            }
            oldevent->ParseIcalComponent( fetchedvevent );
            icalcomponent_free( fetchedvevent );
        } else {
            #ifdef ICAL_DEBUG
            printf( "oeICalImpl::ModifyTodo() - WARNING Event not found.\n" );
            #endif
            nsMemory::Free( *retid );
            *retid = nsnull;
            icalfileset_free(stream);
            return NS_OK;
        }
    } else {
        #ifdef ICAL_DEBUG
        printf( "oeICalImpl::ModifyTodo() - WARNING Event not found.\n" );
        #endif
        nsMemory::Free( *retid );
        *retid = nsnull;
        icalfileset_free(stream);
        return NS_OK;
    }
    
    //Update Last-Modified
    icalevent->UpdateLastModified();

    vcalendar = ((oeICalTodoImpl *)icalevent)->AsIcalComponent();

    icalfileset_add_first_to_first_component( stream, vcalendar );  
      
	if( icalfileset_commit(stream) != ICAL_NO_ERROR ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::ModifyTodo() : WARNING icalfileset_commit() unsuccessful\n" );
        #endif
    }
    icalfileset_free(stream);
    
    PRUint32 observercount;
    m_todoobserverlist->Count( &observercount );
    for( unsigned int i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalTodoObserver>observer;
        m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnModifyItem( icalevent, oldevent );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::ModifyTodo() : WARNING Call to observer's onModifyItem() unsuccessful: %x\n", rv );
        }
        #endif
    }

    oldevent->Release();

    SetupAlarmManager();
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::GetAllTodos(nsISimpleEnumerator **resultList )
{
#ifdef ICAL_DEBUG
    printf( "oeICalImpl::GetAllTodos()\n" );
#endif

    nsCOMPtr<oeEventEnumerator> todoEnum;
    if( !*resultList ) {
        todoEnum = new oeEventEnumerator();
        if (!todoEnum)
            return NS_ERROR_OUT_OF_MEMORY;
        todoEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
    } else
        todoEnum = (oeEventEnumerator *)*resultList;

    TodoList *tmplistptr = &m_todolist;
    while( tmplistptr ) {
        if( tmplistptr->todo ) {
            if( SatisfiesFilter( tmplistptr->todo ) )
                todoEnum->AddEvent( tmplistptr->todo );
        }
        tmplistptr = tmplistptr->next;
    }

    // bump ref count
//    return todoEnum->QueryInterface(NS_GET_IID(nsISimpleEnumerator), (void **)resultList);
    return NS_OK;
}

NS_IMETHODIMP oeICalImpl::ReportError( PRInt16 severity, PRUint32 errorid, const char *errorstring ) {

    if( severity >= oeIICalError::CAL_PROBLEM ) {
        #ifdef ICAL_DEBUG
            printf( "oeICalImpl::ReportError(%d,%x) : %s\n", severity, errorid, errorstring );
        #endif
    } else {
        #ifdef ICAL_DEBUG_ALL
            printf( "oeICalImpl::ReportError(%d,%x) : %s\n", severity, errorid, errorstring );
        #endif
    }

    unsigned int i;
    PRUint32 observercount;
    m_observerlist->Count(&observercount);
    for( i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalObserver>observer;
        m_observerlist->QueryElementAt( i, NS_GET_IID(oeIICalObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnError( severity, errorid, errorstring );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::ReportError() : WARNING Call to observer's onError() unsuccessful: %x\n", rv );
        }
        #endif
    }
    m_todoobserverlist->Count(&observercount);
    for( i=0; i<observercount; i++ ) {
        nsCOMPtr<oeIICalTodoObserver>observer;
        m_todoobserverlist->QueryElementAt( i, NS_GET_IID(oeIICalTodoObserver), getter_AddRefs(observer));
        nsresult rv;
        rv = observer->OnError( severity, errorid, errorstring );
        #ifdef ICAL_DEBUG
        if( NS_FAILED( rv ) ) {
            printf( "oeICalImpl::ReportError() : WARNING Call to observer's onError() unsuccessful: %x\n", rv );
        }
        #endif
    }
    return NS_OK;
}

/*************************************************************************************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/
/*    Filter stuff here                                                                                      */
/*************************************************************************************************************/
/*************************************************************************************************************/

bool oeICalImpl::SatisfiesFilter( oeIICalTodo *comp )
{
    bool result=false;
    if( m_filter && m_filter->m_completed ) {
        if( icaltime_is_null_time( m_filter->m_completed->m_datetime  ) )
            return true;
    }
    oeDateTimeImpl *completed;
    nsresult rv;
    rv = comp->GetCompleted( (oeIDateTime **)&completed );
    if( NS_FAILED( rv ) ) {
        #ifdef ICAL_DEBUG
	    printf( "oeICalImpl::SatisfiesFilter() : WARNING GetCompleted() unsuccessful\n" );
        #endif
        return false;
    }
    if( icaltime_is_null_time( completed->m_datetime ) )
        result = true;
    if( icaltime_compare( completed->m_datetime , m_filter->m_completed->m_datetime ) > 0 )
        result = true;
    NS_RELEASE( completed );
    return result;
}

NS_IMETHODIMP oeICalImpl::GetFilter( oeIICalTodo **retval )
{
    *retval = m_filter;
    NS_ADDREF(*retval);
    return NS_OK;
}

NS_IMETHODIMP oeICalImpl::ResetFilter()
{
    if( m_filter && m_filter->m_completed )
        m_filter->m_completed->m_datetime = icaltime_null_time();
    return NS_OK;
}
  
NS_IMPL_ISUPPORTS1(oeICalFilter, oeIICalTodo)

oeICalFilter::oeICalFilter()
{

    nsresult rv;
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_completed ))) {
        m_completed = nsnull;
	}
}

oeICalFilter::~oeICalFilter()
{
    if( m_completed )
        m_completed->Release();
}

NS_IMETHODIMP oeICalFilter::GetType(Componenttype *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetId(char **aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetId(const char *aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetTitle(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetTitle(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetDescription(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetDescription(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetLocation(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetLocation(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetCategories(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetCategories(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetUrl(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetUrl(const nsACString& aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetPrivateEvent(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetPrivateEvent(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetMethod(eventMethodProperty *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetMethod(eventMethodProperty aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetStatus(eventStatusProperty *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetStatus(eventStatusProperty aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetPriority(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetPriority(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetSyncId(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetSyncId(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAllDay(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::SetAllDay(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAlarm(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetAlarm(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAlarmUnits(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetAlarmUnits(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAlarmLength(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetAlarmLength(PRUint32 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAlarmEmailAddress(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetAlarmEmailAddress(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetInviteEmailAddress(char * *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetInviteEmailAddress(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurInterval(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetRecurInterval(PRUint32 aNewVal )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurCount(PRUint32 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetRecurCount(PRUint32 aNewVal )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurUnits(char **aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetRecurUnits(const char * aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecur(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetRecur(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurForever(PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::SetRecurForever(PRBool aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetLastModified(PRTime *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::UpdateLastModified()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetLastAlarmAck(PRTime *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::SetLastAlarmAck(PRTime aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetNextRecurrence( PRTime begin, PRTime *retval, PRBool *isvalid ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetPreviousOccurrence( PRTime beforethis, PRTime *retval, PRBool *isvalid ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetStart(oeIDateTime * *start)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetEnd(oeIDateTime * *end)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetStamp(oeIDateTime * *stamp)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurEnd(oeIDateTime * *recurend)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurWeekdays(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::SetRecurWeekdays(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetRecurWeekNumber(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetRecurWeekNumber(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetIcalString(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

	NS_IMETHODIMP oeICalFilter::ParseIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::AddException( PRTime exdate )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::RemoveAllExceptions()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetExceptions(nsISimpleEnumerator **datelist )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetSnoozeTime( PRTime snoozetime )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::Clone( oeIICalEvent **ev )
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetDue(oeIDateTime * *due)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetCompleted(oeIDateTime * *completed)
{
    *completed = m_completed;
    NS_ADDREF(*completed);
    return NS_OK;
}

NS_IMETHODIMP oeICalFilter::GetPercent(PRInt16 *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetPercent(PRInt16 aNewVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::AddAttachment(nsIMsgAttachment *attachment)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::RemoveAttachment(nsIMsgAttachment *attachment)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::RemoveAttachments()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetAttachmentsArray(nsISupportsArray * *aAttachmentsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::AddContact(nsIAbCard *contact)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::RemoveContact(nsIAbCard *contact)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::RemoveContacts()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetContactsArray(nsISupportsArray * *aContactsArray)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetParent( oeIICal **parent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetTodoIcalString(nsACString& aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::ParseTodoIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP oeICalFilter::SetDuration(PRBool is_negative, PRUint16 weeks, PRUint16 days, PRUint16 hours, PRUint16 minutes, PRUint16 seconds)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetDuration(PRBool *is_negative, PRUint16 *weeks, PRUint16 *days, PRUint16 *hours, PRUint16 *minutes, PRUint16 *seconds)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::ReportError( PRInt16 severity, PRUint32 errorid, const char *errorstring ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::SetParameter( const char *name, const char *value ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP oeICalFilter::GetParameter( const char *name, char **value ) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

