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

#include <unistd.h>

#define __cplusplus__ 1


#include "oeICalImpl.h"
#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "stdlib.h"                                
extern "C" {
    #include "icalss.h"
}

#define MAXEVENTSTRINGSIZE 500

char serveraddr[200]="/var/calendar/mostafah";
char *idliststr; //global string for returning id list strings

void
cc_searched(unsigned long id)
{
	char tmp[80];
	char *newstr;
	sprintf( tmp, "%lu,", id );
	if( idliststr == NULL ) {
		idliststr = new char[strlen( tmp ) + 1];
		strcpy( idliststr, tmp );
	} else {
		newstr = new char[ strlen( idliststr ) + strlen( tmp ) + 1 ];
		sprintf( newstr, "%s%s", idliststr, tmp );
		delete idliststr;
		idliststr = newstr;
	}
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
        NS_INIT_REFCNT();
}

oeICalImpl::~oeICalImpl()
{

}

char icalrawcalendarstr[] = "BEGIN:VCALENDAR\n\
BEGIN:VEVENT\n\
UID:900000000\n\
END:VEVENT\n\
END:VCALENDAR\n\
";

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
    icalfileset *stream;
    icalcomponent *icalcalendar,*icalevent;
    struct icaltimetype start, end;
    char uidstr[10]="900000000";
    
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);

    icalcalendar = icalparser_parse_string(icalrawcalendarstr);
    assert(icalcalendar != 0);

    icalevent = icalcomponent_get_first_component(icalcalendar,ICAL_VEVENT_COMPONENT);
    assert(icalevent != 0);

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

    icalproperty *classp = icalproperty_new_class( "PRIVATE" );
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

    icalfileset_add_component(stream,icalcalendar);

	icalfileset_commit(stream);
	printf("Appended Event\n");
    
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    assert( fetchedcal != 0 );
	
    icalcomponent *fetchedevent = icalcomponent_get_first_component( fetchedcal,ICAL_VEVENT_COMPONENT);
    assert( fetchedevent != 0 );

    printf("Fetched Event\n");
	
    icalproperty *tmpprop;
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_UID_PROPERTY );
    assert( tmpprop != 0 );
    printf("id: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_SUMMARY_PROPERTY );
    assert( tmpprop != 0 );
    printf("Title: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CATEGORIES_PROPERTY );
    assert( tmpprop != 0 );
    printf("Category: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DESCRIPTION_PROPERTY );
    assert( tmpprop != 0 );
    printf("Description: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_LOCATION_PROPERTY );
    assert( tmpprop != 0 );
    printf("Location: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_CLASS_PROPERTY );
    assert( tmpprop != 0 );
    printf("Class: %s\n", icalproperty_get_value_as_string( tmpprop ) );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTSTART_PROPERTY );
    assert( tmpprop != 0 );
    start = icalproperty_get_dtstart( tmpprop );
    printf("Start: %d-%d-%d %d:%d\n", start.year, start.month, start.day, start.hour, start.minute );
    //
    tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_DTEND_PROPERTY );
    assert( tmpprop != 0 );
    end = icalproperty_get_dtstart( tmpprop );
    printf("End: %d-%d-%d %d:%d\n", end.year, end.month, end.day, end.hour, end.minute );
    //
    for( tmpprop = icalcomponent_get_first_property( fetchedevent, ICAL_X_PROPERTY );
            tmpprop != 0 ;
            tmpprop = icalcomponent_get_next_property( fetchedevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( tmpprop, ICAL_MEMBER_PARAMETER );
            printf("%s: %s\n", icalparameter_get_member( tmppar ), icalproperty_get_value_as_string( tmpprop ) );
    }

    icalcomponent *newcomp = icalcomponent_new_clone( fetchedcal );
    assert( newcomp != 0 );
    icalcomponent *newevent = icalcomponent_get_first_component( newcomp, ICAL_VEVENT_COMPONENT );
    assert( newevent != 0 );
    tmpprop = icalcomponent_get_first_property( newevent, ICAL_SUMMARY_PROPERTY );
    assert( tmpprop != 0 );
    icalproperty_set_summary( tmpprop, "LUNCH AND LEARN TIME" );
//    icalfileset_modify( stream, fetchedcal, newcomp );
    icalfileset_remove_component( stream, fetchedcal );
    icalfileset_add_component( stream, newcomp );
    icalcomponent_free( fetchedcal );
    //

    icalfileset_commit(stream);
	printf("Event updated\n");
    printf( "New Title: %s\n", icalproperty_get_value_as_string( tmpprop ) );

    fetchedcal = icalfileset_fetch( stream, uidstr );
    assert( fetchedcal != 0 );
    icalfileset_remove_component( stream, fetchedcal );
	
    printf("Removed Event\n");

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
    printf( "SetServer()\n" );
#endif
	strcpy( serveraddr, str );

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
NS_IMETHODIMP oeICalImpl::AddEvent(oeIICalEvent *icalcalendar, PRInt32 *retid)
{
#ifdef ICAL_DEBUG
    printf( "AddEvent()\n" );
#endif
	unsigned long   id;
    icalfileset *stream;
    icalcomponent *vcalendar,*fetchedcal;
    char uidstr[10];

	*retid = 0;
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);

    vcalendar = ((oeICalEventImpl *)icalcalendar)->vcalendar;
	
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *uid = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    assert( uid != 0);
    
    do {
        id = 900000000 + ((rand()%0x4ff) << 16) + rand()%0xFFFF;
        sprintf( uidstr, "%lu", id );
    } while ( (fetchedcal = icalfileset_fetch( stream, uidstr )) != 0 );
    
    icalproperty_set_uid( uid, uidstr );
    
    icalcomponent *vcalclone = icalcomponent_new_clone( vcalendar );
    assert(vcalclone != 0);

    icalfileset_add_component( stream, vcalclone );
	*retid = id;

    icalfileset_commit( stream );
    icalfileset_free( stream );
    return NS_OK;
}


/**
*
*   UpdateEvent
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
NS_IMETHODIMP oeICalImpl::UpdateEvent(oeIICalEvent *icalcalendar, PRInt32 *retid)
{
#ifdef ICAL_DEBUG
    printf( "UpdateEvent()\n" );
#endif
	unsigned long id;
    icalfileset *stream;
    icalcomponent *vcalendar,*vcalclone;
    char uidstr[10];

    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    vcalendar = ((oeICalEventImpl *)icalcalendar)->vcalendar;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    assert( prop != 0);
    
    id = atol( icalproperty_get_value_as_string( prop ) );

    sprintf( uidstr, "%lu", id );
    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    
    if( fetchedcal ) {
        icalfileset_remove_component( stream, fetchedcal );
        icalcomponent_free( fetchedcal );
    }
    
    vcalclone = icalcomponent_new_clone( vcalendar );

    icalfileset_add_component( stream, vcalclone );
    
    *retid = id;
	
    icalfileset_commit( stream );
    icalfileset_free(stream);
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
#ifdef ICAL_DEBUG
    printf( "FetchEvent()\n" );
#endif

    icalfileset *stream;
    char uidstr[10];
    
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    sprintf( uidstr, "%lu", id );

    nsresult rv;
	oeICalEventImpl *icalevent;
	if( NS_FAILED( rv = NS_NewICalEvent((oeIICalEvent**) &icalevent ))) {
	        *ev = NULL;
		return rv;
	}
	icalcomponent_free( icalevent->vcalendar );
	
    icalcomponent *tmpcomp = icalfileset_fetch( stream, uidstr );
    if( tmpcomp == 0 ) {
		printf("FetchEvent() failed\n");
        *ev = NULL;
		return NS_OK;
    }

    icalevent->vcalendar = icalcomponent_new_clone( tmpcomp );

	*ev = icalevent;

    icalfileset_free(stream);

    return NS_OK;
}

/**
*
*   SearchEvent
*
*   DESCRIPTION: Searches for events
*
*   PARAMETERS: 
*	startstr	- String representation of the start date-time
*	endstr		- String representation of the end date-time
*	resultstr   - Place to store the string representation of the id list
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::SearchEvent( PRInt16 startyear, PRInt16 startmonth, PRInt16 startday,
				 PRInt16 starthour, PRInt16 startmin,
			  	 PRInt16 endyear, PRInt16 endmonth, PRInt16 endday,
				 PRInt16 endhour, PRInt16 endmin,
			  	 char **resultstr )
{
#ifdef ICAL_DEBUG
    printf( "SearchEvent()\n" );
#endif
    icalgauge* gauge;
    char sqlstr[200];
    icalfileset *stream;
    
    idliststr = NULL;
    
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    sprintf( sqlstr, "SELECT * FROM VEVENT WHERE DTSTART >= '%04d%02d%02dT%02d%02d00' and DTEND < '%04d%02d%02dT%02d%02d00'",
             startyear,startmonth,startday,starthour,startmin,
             endyear,endmonth,endday,endhour,endmin );
    gauge = icalgauge_new_from_sql( sqlstr );
    assert( gauge != 0 );
    
    icalfileset_select( stream, gauge );

    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        icalcomponent *vevent = icalcomponent_get_first_component( comp, ICAL_VEVENT_COMPONENT );
        assert( vevent != 0);

        icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
        assert( prop != 0);

        cc_searched( atol( icalproperty_get_value_as_string( prop ) ) );
    }
	
    *resultstr = idliststr;

    icalfileset_free(stream);
    return NS_OK;
}

NS_IMETHODIMP
oeICalImpl::SearchForEvent( const char *sqlstr, char **resultstr )
{
#ifdef ICAL_DEBUG
    printf( "SearchForEvent()\n" );
#endif
    icalgauge* gauge;
    icalfileset *stream;
    
    idliststr = NULL;
    
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    gauge = icalgauge_new_from_sql( (char *)sqlstr );
    assert( gauge != 0 );
    
    icalfileset_select( stream, gauge );

    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        icalcomponent *vevent = icalcomponent_get_first_component( comp, ICAL_VEVENT_COMPONENT );
        assert( vevent != 0);

        icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
        assert( prop != 0);

        cc_searched( atol( icalproperty_get_value_as_string( prop ) ) );
    }
	
    *resultstr = idliststr;

    icalfileset_free(stream);
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
    printf( "DeleteEvent()\n" );
#endif
    icalfileset *stream;
    char uidstr[10];
    
    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    sprintf( uidstr, "%lu", id );

    icalcomponent *fetchedcal = icalfileset_fetch( stream, uidstr );
    
    if( fetchedcal == 0 ) {
        icalfileset_free(stream);
        return NS_OK;
    }
    
    icalfileset_remove_component( stream, fetchedcal );
	icalcomponent_free( fetchedcal );
	
    icalfileset_commit( stream );
    icalfileset_free(stream);
	
	return NS_OK;
}

/**
*
*   SnoozeEvent
*
*   DESCRIPTION: Deactivates an alarm of an event
*
*   PARAMETERS: 
*	id		- Id of the event to deactivet its alarm
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::SnoozeEvent( PRInt32 id )
{
/*    cal_open(serveraddr, 0);
	if( 0 ) {
		printf("cal_open() failed - SnoozeEvent\n");
		return 0;
	}

	if (!cal_snooze(id)) {
		printf("cal_snooze() failed\n");
		return 0;
	}
	
	cal_close(0);
	if ( 0 ) {
		printf("cal_close() failed\n");
		return 0;
	}
*/	
	return NS_OK;
}

void GetNextRecurrence( icalcomponent *vevent, PRInt16 year, PRInt16 month, PRInt16 day, struct icaldurationtype before, char *aRetVal ) {

    struct icaltimetype start, estart, next;
    struct icalrecurrencetype recur;
    icalrecur_iterator* ritr;

    start.year = year; start.month = month; start.day = day;
    start.hour = 0; start.minute = 0; start.second = 0;

    icalproperty *dtstart = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if( dtstart != 0 ) {
        estart = icalproperty_get_dtstart( dtstart );
        start.hour = estart.hour; start.minute = estart.minute; start.second = estart.second;
    }

    aRetVal[0]= 0;
    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        recur = icalproperty_get_rrule(prop);
        ritr = icalrecur_iterator_new(recur,estart);
        for(next = icalrecur_iterator_next(ritr);
            !icaltime_is_null_time(next);
            next = icalrecur_iterator_next(ritr)){
            struct icaltimetype tmpnext = icaltime_add( next, before );
            if( icaltime_compare( tmpnext , start ) >= 0 ) {
                sprintf( aRetVal, "%04d%02d%02dT%02d%02d%02d" , next.year, next.month, next.day, next.hour, next.minute, next.second );
                break;
            }
        }
    }
}

/**
*
*   SearchAlarm
*
*   DESCRIPTION: Searches for events having active alarms
*
*   PARAMETERS: 
*	whenstr		- String representation of the date-time reference of the alarm
*	resultstr   - Place to store the string representation of the id list
*
*   RETURN
*      None
*/
NS_IMETHODIMP
oeICalImpl::SearchAlarm( PRInt16 startyear, PRInt16 startmonth, PRInt16 startday,
				 PRInt16 starthour, PRInt16 startmin,
			  	 char **resultstr )
{
#ifdef ICAL_DEBUG
    printf( "SearchAlarm()\n" );
#endif
    icalfileset *stream;
    icaltimetype check,checkend;
    icaldurationtype checkduration;
    icalproperty *prop;

    idliststr = NULL;
    
    check.year = startyear; check.month = startmonth; check.day = startday; 
    check.hour = starthour; check.minute = startmin; check.second = 0; 
    printf( "CHECK: %d-%d-%d %d:%d:%d\n", check.year, check.month, check.day, check.hour, check.minute, check.second );
    checkduration.is_neg = false;
    checkduration.weeks = checkduration.days = checkduration.minutes = checkduration.seconds = 0;
    checkduration.hours = 24;
    checkend = icaltime_add( check, checkduration );
    printf( "CHECKEND: %d-%d-%d %d:%d:%d\n", checkend.year, checkend.month, checkend.day, checkend.hour, checkend.minute, checkend.second );

    stream = icalfileset_new(serveraddr);
    assert(stream != 0);
    
    icalcomponent *comp;
    for( comp = icalfileset_get_first_component( stream );
        comp != 0;
        comp = icalfileset_get_next_component( stream ) ) {
        icalcomponent *vevent = icalcomponent_get_first_component( comp, ICAL_VEVENT_COMPONENT );
        assert( vevent != 0);

        icalcomponent *valarm = icalcomponent_get_first_component( vevent, ICAL_VALARM_COMPONENT );
        if( valarm ){
            struct icaltimetype start;

            //get duration 
            prop = icalcomponent_get_first_property( valarm, ICAL_TRIGGER_PROPERTY );
            assert( prop != 0);
            struct icaltriggertype trig;
            trig = icalproperty_get_trigger( prop );
            
            prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
            if ( prop != 0) {
                char result[80];
                GetNextRecurrence( vevent, check.year, check.month, check.day , trig.duration, result );
                start = icaltime_from_string( result );
                printf( "RECUR START: %d-%d-%d %d:%d:%d\n", start.year, start.month, start.day, start.hour, start.minute, start.second );
            } else {
                //get dtstart
                prop = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
                assert( prop != 0);
                start = icalproperty_get_dtstart( prop );
                printf( "START: %d-%d-%d %d:%d:%d\n", start.year, start.month, start.day, start.hour, start.minute, start.second );
            }
            start = icaltime_add( start, trig.duration );
            printf( "TRIGGER: %d-%d-%d %d:%d:%d\n", start.year, start.month, start.day, start.hour, start.minute, start.second );
            if( icaltime_compare( start, check ) >= 0 && icaltime_compare( start, checkend ) < 0 ) {
                prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
                assert( prop != 0);

                cc_searched( atol( icalproperty_get_value_as_string( prop ) ) );
            }
        }
    }
	
    *resultstr = idliststr;

    icalfileset_free(stream);
    return NS_OK;
}
