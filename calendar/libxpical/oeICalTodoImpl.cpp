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
 * The Original Code is Mozilla Calendar Code.
 *
 * The Initial Developer of the Original Code is
 * Chris Charabaruk <ccharabaruk@meldstar.com>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 Mostafa Hosseini <mostafah@oeone.com>
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

/* This file implements an XPCOM object which represents a calendar task(todo) object. It is a derivation
of the event object which adds fields exclusive to tasks.
*/

#ifndef WIN32
#include <unistd.h>
#endif

#include "oeICalEventImpl.h"
#include "oeICalTodoImpl.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
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

/* Implementation file */
NS_IMPL_ISUPPORTS2(oeICalTodoImpl, oeIICalTodo, oeIICalEvent)

icaltimetype ConvertFromPrtime( PRTime indate );
PRTime ConvertToPrtime ( icaltimetype indate );

nsresult
NS_NewICalTodo( oeIICalTodo** inst )
{
    NS_PRECONDITION(inst != nsnull, "null ptr");
    if (! inst)
        return NS_ERROR_NULL_POINTER;

    *inst = new oeICalTodoImpl();
    if (! *inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*inst);
    return NS_OK;
}

oeICalTodoImpl::oeICalTodoImpl()
{
#ifdef ICAL_DEBUG
    printf( "oeICalTodoImpl::oeICalTodoImpl()\n" );
#endif

    mEvent = new oeICalEventImpl();
    NS_ADDREF( mEvent );

    mEvent->SetType( XPICAL_VTODO_COMPONENT );

    /* member initializers and constructor code */
    nsresult rv;
	if( NS_FAILED( rv = NS_NewDateTime((oeIDateTime**) &m_completed ))) {
        m_completed = nsnull;
	}
    m_percent = 0;

    //Some defaults are different for todos, apply them
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    if ( NS_SUCCEEDED(rv) && prefBranch ) {
        nsXPIDLCString tmpstr;
        PRInt32 tmpint;
        rv = prefBranch->GetIntPref("calendar.alarms.onfortodos", &tmpint);
        if (NS_SUCCEEDED(rv))
            SetAlarm( tmpint );
        rv = prefBranch->GetIntPref("calendar.alarms.todoalarmlen", &tmpint);
        if (NS_SUCCEEDED(rv))
            SetAlarmLength( tmpint );
        rv = prefBranch->GetCharPref("calendar.alarms.todoalarmunit", getter_Copies(tmpstr));
        if (NS_SUCCEEDED(rv))
            SetAlarmUnits( PromiseFlatCString( tmpstr ).get() );
    }
}

oeICalTodoImpl::~oeICalTodoImpl()
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalTodoImpl::~oeICalTodoImpl()\n");
#endif
    /* destructor code */
    if( m_completed )
        m_completed->Release();
    mEvent->Release();
}

bool oeICalTodoImpl::matchId( const char *id ) {
    return mEvent->matchId( id );
}

/* readonly attribute oeIDateTime due; */
NS_IMETHODIMP oeICalTodoImpl::GetDue(oeIDateTime * *due)
{
    return mEvent->GetEnd( due );
}

/* areadonly attribute oeIDateTime completed; */
NS_IMETHODIMP oeICalTodoImpl::GetCompleted(oeIDateTime * *completed)
{
    *completed = m_completed;
    NS_ADDREF(*completed);
    return NS_OK;
}

/* attribute short percent; */
NS_IMETHODIMP oeICalTodoImpl::GetPercent(PRInt16 *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "GetPercent() = " );
#endif

    *aRetVal = m_percent;

#ifdef ICAL_DEBUG_ALL
    printf( "%d\n", *aRetVal );
#endif
    return NS_OK;
}
NS_IMETHODIMP oeICalTodoImpl::SetPercent(PRInt16 aNewVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "SetPercent( %d )\n", aNewVal );
#endif
    m_percent = aNewVal;
    return NS_OK;
}

NS_IMETHODIMP oeICalTodoImpl::Clone( oeIICalTodo **ev )
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalTodoImpl::Clone()\n" );
#endif
    nsresult rv;
    oeICalTodoImpl *icaltodo =nsnull;
    if( NS_FAILED( rv = NS_NewICalTodo( (oeIICalTodo**) &icaltodo ) ) ) {
        return rv;
    }
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::Clone() failed!\n" );
        #endif
        icaltodo->Release();
        return NS_OK;
    }
    icalcomponent *vtodo = icalcomponent_get_first_component( vcalendar, ICAL_VTODO_COMPONENT );
    if( !(icaltodo->ParseIcalComponent( vtodo )) ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::Clone() failed.\n" );
        #endif
        icaltodo->Release();
        return NS_OK;
    }
    *ev = icaltodo;
    return NS_OK;
}

NS_IMETHODIMP oeICalTodoImpl::GetTodoIcalString(nsACString& aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalTodoImpl::GetTodoIcalString() = " );
#endif
    
    icalcomponent *vcalendar = AsIcalComponent();
    if ( !vcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::GetTodoIcalString() failed!\n" );
        #endif
        return NS_OK;
    }

    char *str = icalcomponent_as_ical_string( vcalendar );
    if( str ) {
		aRetVal = str;
    } else
        aRetVal.Truncate();
    icalcomponent_free( vcalendar );

#ifdef ICAL_DEBUG_ALL
    printf( "\"%s\"\n", PromiseFlatCString( aRetVal ).get() );
#endif
    return NS_OK;
}

NS_IMETHODIMP oeICalTodoImpl::ParseTodoIcalString(const nsACString& aNewVal, PRBool *aRetVal)
{
#ifdef ICAL_DEBUG_ALL
    printf( "oeICalTodoImpl::ParseTodoIcalString( %s )\n", PromiseFlatCString( aNewVal ).get() );
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

bool oeICalTodoImpl::ParseIcalComponent( icalcomponent *comp )
{
#ifdef ICAL_DEBUG_ALL
    printf( "ParseIcalComponent()\n" );
#endif

    icalcomponent *vtodo=nsnull;
    icalcomponent_kind kind = icalcomponent_isa( comp );

    if( kind == ICAL_VCALENDAR_COMPONENT )
	    vtodo = icalcomponent_get_first_component( comp , ICAL_VTODO_COMPONENT );
    else if( kind == ICAL_VTODO_COMPONENT )
        vtodo = comp;

    if ( !vtodo ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::ParseIcalComponent() failed: vtodo is NULL!\n" );
        #endif
        return false;
    }

    //First parse all basic properties
    ((oeICalEventImpl *)mEvent)->ParseIcalComponent( vtodo );

    //then go on with the extra properties

    //percent
    icalproperty *prop = icalcomponent_get_first_property( vtodo, ICAL_PERCENTCOMPLETE_PROPERTY );
    if ( prop != 0) {
        m_percent = icalproperty_get_percentcomplete( prop );
    } else {
        m_percent = 0;
    }

    //completed
    prop = icalcomponent_get_first_property( vtodo, ICAL_COMPLETED_PROPERTY );
    if (prop != 0) {
        icaltimetype completed;
        completed = icalproperty_get_completed( prop );
        m_completed->m_datetime = completed;
    } else {
        m_completed->m_datetime = icaltime_null_time();
    }

    return true;
}

icalcomponent* oeICalTodoImpl::AsIcalComponent()
{
#ifdef ICAL_DEBUG_ALL
    printf( "AsIcalComponent()\n" );
#endif
    icalcomponent *newcalendar;
    
    newcalendar = icalcomponent_new_vcalendar();
    if ( !newcalendar ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::AsIcalComponent() failed: Cannot create VCALENDAR!\n" );
        #endif
        return nsnull;
    }
    icalcomponent *basevcal = ((oeICalEventImpl *)mEvent)->AsIcalComponent();
    if ( !basevcal ) {
        #ifdef ICAL_DEBUG
        printf( "oeICalTodoImpl::AsIcalComponent() failed: Cannot create ical component from mEvent!\n" );
        #endif
        icalcomponent_free( newcalendar );
        return nsnull;
    }
    
    //version
    icalproperty *prop = icalproperty_new_version( ICALEVENT_VERSION );
    icalcomponent_add_property( newcalendar, prop );

    //prodid
    prop = icalproperty_new_prodid( ICALEVENT_PRODID );
    icalcomponent_add_property( newcalendar, prop );
    icalcomponent *vtodo = icalcomponent_new_vtodo();
    icalcomponent *vevent = icalcomponent_get_first_component( basevcal, ICAL_VEVENT_COMPONENT );
    for( prop = icalcomponent_get_first_property( vevent, ICAL_ANY_PROPERTY );
         prop != 0 ;
         prop = icalcomponent_get_next_property( vevent, ICAL_ANY_PROPERTY ) ) {
        icalproperty *newprop;
        icalproperty_kind propkind = icalproperty_isa( prop );
        if( propkind == ICAL_X_PROPERTY ) {
            newprop = icalproperty_new_x( icalproperty_get_value_as_string( prop ) );
            icalproperty_set_x_name( newprop, icalproperty_get_x_name( prop ));
            icalparameter *oldpar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            icalparameter *newpar = icalparameter_new_member( icalparameter_get_member( oldpar ) );
            icalproperty_add_parameter( newprop, newpar );
        } else if( propkind == ICAL_DTEND_PROPERTY ) {
            //Change DTEND to DUE
            newprop = icalproperty_new_due( icalproperty_get_dtend( prop ) );
            icalparameter *oldpar = icalproperty_get_first_parameter( prop, ICAL_TZID_PARAMETER );
            if( oldpar ) {
                icalparameter *newpar = icalparameter_new_tzid( icalparameter_get_tzid( oldpar ) );
                icalproperty_add_parameter( newprop, newpar );
            }
        } else {
            newprop = icalproperty_new_clone( prop );
        }
        icalcomponent_add_property( vtodo, newprop );
    }
    icalcomponent *nestedcomp;
    for( nestedcomp = icalcomponent_get_first_component( vevent, ICAL_ANY_COMPONENT );
         nestedcomp != 0 ;
         nestedcomp = icalcomponent_get_next_component( vevent, ICAL_ANY_COMPONENT ) ) {
        icalcomponent_add_component( vtodo, icalcomponent_new_clone(nestedcomp) );
    }
    icalcomponent_free( basevcal );
    //percent
    if( m_percent != 0) {
        prop = icalproperty_new_percentcomplete( m_percent );
        icalcomponent_add_property( vtodo, prop );
    }

    //completed
    if( m_completed && !icaltime_is_null_time( m_completed->m_datetime ) ) {
        prop = icalproperty_new_completed( m_completed->m_datetime );
        icalcomponent_add_property( vtodo, prop );
    }

    //add event to newcalendar
    icalcomponent_add_component( newcalendar, vtodo );

    return newcalendar;
}

oeICalEventImpl *oeICalTodoImpl::GetBaseEvent() {
    return mEvent;
}

/* End of implementation class template. */
