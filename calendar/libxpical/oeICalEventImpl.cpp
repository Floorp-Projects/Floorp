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

#include <unistd.h>

#include "oeICalEventImpl.h"
#include "nsMemory.h"
#include "stdlib.h"


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
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
    vcalendar = icalcomponent_new_vcalendar();
    icalcomponent *vevent = icalcomponent_new_vevent();
    icalproperty *uid = icalproperty_new_uid( "900000000" );
    icalcomponent_add_property( vevent, uid );
    icalcomponent_add_component( vcalendar, vevent );
    assert(vcalendar != 0);
}

oeICalEventImpl::~oeICalEventImpl()
{
  /* destructor code */
  if( vcalendar != NULL )
	icalcomponent_free( vcalendar );
}

/* attribute long Id; */
NS_IMETHODIMP oeICalEventImpl::GetId(PRUint32 *aId)
{
#ifdef ICAL_DEBUG
    printf( "GetId()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    assert( prop != 0);

    *aId = atol( icalproperty_get_value_as_string( prop ) );
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetId(PRUint32 aId)
{
#ifdef ICAL_DEBUG
    printf( "SetId(%lu)\n", aId );
#endif
    char uidstr[10];
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_UID_PROPERTY );
    assert( prop != 0);

    sprintf( uidstr, "%lu" , aId );
    
    icalproperty_set_uid( prop, uidstr );
    return NS_OK;
}

/* attribute string Title; */
NS_IMETHODIMP oeICalEventImpl::GetTitle(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetTitle()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_SUMMARY_PROPERTY );
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal= (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetTitle(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetTitle()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_SUMMARY_PROPERTY );
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_summary( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ){
        prop = icalproperty_new_summary( aNewVal );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute string Description; */
NS_IMETHODIMP oeICalEventImpl::GetDescription(char * *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetDescription()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DESCRIPTION_PROPERTY );
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetDescription(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetDescription()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DESCRIPTION_PROPERTY );
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_description( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        prop = icalproperty_new_description( aNewVal );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute string Location; */
NS_IMETHODIMP oeICalEventImpl::GetLocation(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetLocation()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_LOCATION_PROPERTY );
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
   return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetLocation(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetLocation()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_LOCATION_PROPERTY );
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_location( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        prop = icalproperty_new_location( aNewVal );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute string Category; */
NS_IMETHODIMP oeICalEventImpl::GetCategory(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetCategory()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_CATEGORIES_PROPERTY );
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetCategory(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetCategory()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_CATEGORIES_PROPERTY );
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_categories( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        prop = icalproperty_new_categories( aNewVal );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute boolean PrivateEvent; */
NS_IMETHODIMP oeICalEventImpl::GetPrivateEvent(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetPrivateEvent()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_CLASS_PROPERTY );
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        if( strcmp( str, "PUBLIC" ) == 0 )
            *aRetVal= false;
        else
            *aRetVal= true;
    } else
        *aRetVal= true;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetPrivateEvent(PRBool aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetPrivateEvent()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_CLASS_PROPERTY );
    if( prop != 0 ) {
        if( aNewVal )
            icalproperty_set_class( prop, "PRIVATE" );
        else
            icalproperty_set_class( prop, "PUBLIC" );
    } else {
        if( aNewVal )
            prop = icalproperty_new_class( "PRIVATE" );
        else
            prop = icalproperty_new_class( "PUBLIC" );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute boolean AllDay; */
NS_IMETHODIMP oeICalEventImpl::GetAllDay(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetAllDay()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AllDay" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        if( strcmp( str, "TRUE" ) == 0 )
            *aRetVal= true;
        else
            *aRetVal= false;
    } else
        *aRetVal= false;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetAllDay(PRBool aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetAllDay()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AllDay" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( aNewVal )
            icalproperty_set_x( prop, "TRUE" );
        else
            icalproperty_set_x( prop, "FALSE" );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "AllDay" );
        if( aNewVal )
            prop = icalproperty_new_x( "TRUE" );
        else
            prop = icalproperty_new_x( "FALSE" );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    
    return NS_OK;
}

/* attribute boolean Alarm; */
NS_IMETHODIMP oeICalEventImpl::GetAlarm(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetAlarm()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalcomponent *valarm;
    valarm = icalcomponent_get_first_component( vevent, ICAL_VALARM_COMPONENT );
    
    if ( valarm != 0)
        *aRetVal= true;
    else
        *aRetVal= false;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarm(PRBool aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetAlarm()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalcomponent *valarm;
    valarm = icalcomponent_get_first_component( vevent, ICAL_VALARM_COMPONENT );
    
    if( valarm != 0 ) {
        icalcomponent_remove_component( vevent, valarm );
        icalcomponent_free( valarm );
    }

    if( aNewVal ) {
        struct icaltriggertype trig;
        valarm = icalcomponent_new_valarm();
        trig.time.year = trig.time.month = trig.time.day = trig.time.hour = trig.time.minute = trig.time.second = 0;
        trig.duration.is_neg = true;
        trig.duration.days = trig.duration.weeks = trig.duration.hours = trig.duration.seconds = 0;
        PRUint32 alarmlength;
        GetAlarmLength( &alarmlength );
        trig.duration.minutes = alarmlength;
        icalproperty *prop = icalproperty_new_trigger( trig );
        icalcomponent_add_property( valarm, prop );
        icalcomponent_add_component( vevent, valarm );
    }
    return NS_OK;
}

/* attribute boolean AlarmWentOff; */
/*NS_IMETHODIMP oeICalEventImpl::GetAlarmWentOff(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetAlarmWentOff()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmWentOff" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        if( strcmp( str, "TRUE" ) == 0 )
            *aRetVal= true;
        else
            *aRetVal= false;
    } else
        *aRetVal= false;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetAlarmWentOff(PRBool aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetAlarmWentOff()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmWentOff" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( aNewVal )
            icalproperty_set_x( prop, "TRUE" );
        else
            icalproperty_set_x( prop, "FALSE" );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "AlarmWentOff" );
        if( aNewVal )
            prop = icalproperty_new_x( "TRUE" );
        else
            prop = icalproperty_new_x( "FALSE" );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}*/

/* attribute long AlarmLength; */
NS_IMETHODIMP oeICalEventImpl::GetAlarmLength(PRUint32 *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetAlarmLength()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmLength" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal= atol( str );
    } else
        *aRetVal= 0;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarmLength(PRUint32 aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetAlarmLength()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmLength" ) == 0 )
                    break;
            }
    }
    
    char tmpstr[20];
    sprintf( tmpstr, "%lu", aNewVal );
    if( prop != 0 ) {
        icalproperty_set_x( prop, tmpstr );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "AlarmLength" );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }

    PRBool isactive;
    GetAlarm( &isactive );
    if( isactive )
        SetAlarm( true );
    return NS_OK;
}

/* attribute string AlarmEmailAddress; */
NS_IMETHODIMP oeICalEventImpl::GetAlarmEmailAddress(char * *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetAlarmEmailAddres()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmEmailAddress" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
   return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetAlarmEmailAddress(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetAlarmEmailAddres()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "AlarmEmailAddress" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_x( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        icalparameter *tmppar = icalparameter_new_member( "AlarmEmailAddress" );
        prop = icalproperty_new_x( aNewVal );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute string InviteEmailAddress; */
NS_IMETHODIMP oeICalEventImpl::GetInviteEmailAddress(char * *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetInviteEmailAddres()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "InviteEmailAddress" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
   return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetInviteEmailAddress(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetInviteEmailAddres()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "InviteEmailAddress" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_x( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        icalparameter *tmppar = icalparameter_new_member( "InviteEmailAddress" );
        prop = icalproperty_new_x( aNewVal );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute string SnoozeTime; */
NS_IMETHODIMP oeICalEventImpl::GetSnoozeTime(char * *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetSnoozeTime()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "SnoozeTime" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
	return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetSnoozeTime(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetSnoozeTime()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "SnoozeTime" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_x( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        icalparameter *tmppar = icalparameter_new_member( "SnoozeTime" );
        prop = icalproperty_new_x( aNewVal );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute boolean RecurType; */
NS_IMETHODIMP oeICalEventImpl::GetRecurType(PRInt16 *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetRecurType()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RecurType" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal= atol( str );
    } else
        *aRetVal= 0;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurType(PRInt16 aNewVal )
{
#ifdef ICAL_DEBUG
    printf( "SetRecurType()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RecurType" ) == 0 )
                    break;
            }
    }
    
    char tmpstr[20];
    sprintf( tmpstr, "%lu", aNewVal );
    if( prop != 0 ) {
        icalproperty_set_x( prop, tmpstr );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "RecurType" );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute boolean RecurInterval; */
NS_IMETHODIMP oeICalEventImpl::GetRecurInterval(PRUint32 *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetRecurInterval()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RecurInterval" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal= atol( str );
    } else
        *aRetVal= 0;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRecurInterval(PRUint32 aNewVal )
{
#ifdef ICAL_DEBUG
    printf( "SetRecurInterval()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RecurInterval" ) == 0 )
                    break;
            }
    }
    
    char tmpstr[20];
    sprintf( tmpstr, "%lu", aNewVal );
    if( prop != 0 ) {
        icalproperty_set_x( prop, tmpstr );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "RecurInterval" );
        prop = icalproperty_new_x( tmpstr );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* string GetRecurEndDate (); */
NS_IMETHODIMP oeICalEventImpl::GetRecurEndDate(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetRecurEndDate()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        struct icalrecurrencetype recur;
        recur = icalproperty_get_rrule(prop);
        char tmp[80];
        sprintf( tmp, "%d,%d,%d,%d,%d." , recur.until.year, recur.until.month, recur.until.day, 0, 0 );
        *aRetVal= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
        if( aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}

#define RECUR_NONE 0
#define RECUR_DAILY 1
#define RECUR_WEEKLY 2
#define RECUR_MONTHLY_MDAY 3
#define RECUR_MONTHLY_WDAY 4
#define RECUR_YEARLY 5

NS_IMETHODIMP oeICalEventImpl::SetRecurInfo( PRInt16 type, PRUint32 interval, PRInt16 year, PRInt16 month, PRInt16 day ) {
#ifdef ICAL_DEBUG
    printf( "SetRecurInfo()\n" );
#endif
    struct icalrecurrencetype recur;

    SetRecurType( type );
    SetRecurInterval( interval );

    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        icalcomponent_remove_property( vcalendar, prop );
        icalproperty_free( prop );
    }

    icalrecurrencetype_clear( &recur );
    
//    for( int i=0; i<ICAL_BY_MONTH_SIZE; i++ )
//        recur.by_month[i] = i;
    recur.interval = interval;

    recur.until.year = year;
    recur.until.month = month;
    recur.until.day = day;
    recur.until.hour = 23;
    recur.until.minute = 59;
    recur.until.second = 59;
    recur.until.is_utc = false;
    recur.until.is_date = true;

    switch (type) {
        case RECUR_NONE:
            break;
        case RECUR_DAILY:
            recur.freq = ICAL_DAILY_RECURRENCE;
            prop = icalproperty_new_rrule( recur );
            icalcomponent_add_property( vevent, prop );
            break;
        case RECUR_WEEKLY:
            recur.freq = ICAL_WEEKLY_RECURRENCE;
            prop = icalproperty_new_rrule( recur );
            icalcomponent_add_property( vevent, prop );
            break;
        case RECUR_MONTHLY_MDAY:
            recur.freq = ICAL_MONTHLY_RECURRENCE;
            prop = icalproperty_new_rrule( recur );
            icalcomponent_add_property( vevent, prop );
            break;
        case RECUR_MONTHLY_WDAY:
            recur.freq = ICAL_MONTHLY_RECURRENCE;
            prop = icalproperty_new_rrule( recur );
            icalcomponent_add_property( vevent, prop );
            break;
        case RECUR_YEARLY:
            icalproperty *dtstart = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
            if ( dtstart != 0) {
                icaltimetype start = icalproperty_get_dtstart( dtstart );
                recur.by_month[0] = start.month;
            }
            recur.freq = ICAL_YEARLY_RECURRENCE;
            prop = icalproperty_new_rrule( recur );
            icalcomponent_add_property( vevent, prop );
            break;
    }
	return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::GetNextRecurrence( PRInt16 year, PRInt16 month, PRInt16 day, char **aRetVal) {
#ifdef ICAL_DEBUG
    printf( "GetNextRecurrence()\n" );
#endif
    struct icaltimetype start, estart, next;
    struct icalrecurrencetype recur;
    icalrecur_iterator* ritr;

    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    month++;
    
    start.year = year; start.month = month; start.day = day;
    start.hour = 0; start.minute = 0; start.second = 0;
    start.is_date = true;

    icalproperty *dtstart = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if( dtstart != 0 ) {
        estart = icalproperty_get_dtstart( dtstart );
        start.hour = estart.hour; start.minute = estart.minute; start.second = estart.second;
//        if( icaltime_compare( estart, start ) > 0 ) {
//            start.year = estart.year; start.month = estart.month; start.day = estart.day;
//        }
    }

    *aRetVal= NULL;
    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_RRULE_PROPERTY );
    if ( prop != 0) {
        recur = icalproperty_get_rrule(prop);
        ritr = icalrecur_iterator_new(recur,estart);
        for(next = icalrecur_iterator_next(ritr);
            !icaltime_is_null_time(next);
            next = icalrecur_iterator_next(ritr)){

            next.is_date = true;
//            printf( "%d-%d-%d %d:%d:%d\n" , next.year, next.month, next.day, next.hour, next.minute, next.second );
//            printf( "%d-%d-%d %d:%d:%d\n" , start.year, start.month, start.day, start.hour, start.minute, start.second );
            if( icaltime_compare( next , start ) > 0 ) {
                char tmp[80];
                sprintf( tmp, "%d,%d,%d,%d,%d." , next.year, next.month-1,
                                         next.day, 0, 0 );
//                printf( "     Result:%s\n", tmp );
                *aRetVal= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
                break;
            }
        }
    }
    return NS_OK;
}

/* attribute string RepeatUnits; */

NS_IMETHODIMP oeICalEventImpl::GetRepeatUnits(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetRepeatUnits()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RepeatUnits" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        *aRetVal = (char*) nsMemory::Clone( str, strlen(str)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRepeatUnits(const char * aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetRepeatUnits()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RepeatUnits" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( strlen( aNewVal ) == 0 ) {
            icalcomponent_remove_property( vevent, prop );
            icalproperty_free( prop );
        } else 
            icalproperty_set_x( prop, aNewVal );
    } else if( strlen( aNewVal ) != 0 ) {
        icalparameter *tmppar = icalparameter_new_member( "RepeatUnits" );
        prop = icalproperty_new_x( aNewVal );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* attribute boolean RepeatForever; */

NS_IMETHODIMP oeICalEventImpl::GetRepeatForever(PRBool *aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetRepeatForever()\n" );
#endif
    char *str;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RepeatForever" ) == 0 )
                    break;
            }
    }
    
    if ( prop != 0) {
        str = (char *)icalproperty_get_value_as_string( prop );
        if( strcmp( str, "TRUE" ) == 0 )
            *aRetVal= true;
        else
            *aRetVal= false;
    } else
        *aRetVal= false;
    return NS_OK;
}
NS_IMETHODIMP oeICalEventImpl::SetRepeatForever(PRBool aNewVal)
{
#ifdef ICAL_DEBUG
    printf( "SetRepeatForever()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop;
    for( prop = icalcomponent_get_first_property( vevent, ICAL_X_PROPERTY );
            prop != 0 ;
            prop = icalcomponent_get_next_property( vevent, ICAL_X_PROPERTY ) ) {
            icalparameter *tmppar = icalproperty_get_first_parameter( prop, ICAL_MEMBER_PARAMETER );
            if ( tmppar != 0 ) {
                char *tmpparstr = (char *)icalparameter_get_member( tmppar );
                if( strcmp( tmpparstr, "RepeatForever" ) == 0 )
                    break;
            }
    }
    
    if( prop != 0 ) {
        if( aNewVal )
            icalproperty_set_x( prop, "TRUE" );
        else
            icalproperty_set_x( prop, "FALSE" );
    } else {
        icalparameter *tmppar = icalparameter_new_member( "RepeatForever" );
        if( aNewVal )
            prop = icalproperty_new_x( "TRUE" );
        else
            prop = icalproperty_new_x( "FALSE" );
        icalproperty_add_parameter( prop, tmppar );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetStartDate( PRInt16 year, PRInt16 month, PRInt16 day, PRInt16 hour, PRInt16 min) {
#ifdef ICAL_DEBUG
    printf( "SetStartDate()\n" );
#endif
    struct icaltimetype start;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);
    
    start.year = year; start.month = month; start.day = day;
    start.hour = hour; start.minute = min; start.second = 0;
    start.is_utc = false; start.is_date = false;

    icalproperty *dtstart = icalproperty_new_dtstart( start );

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if ( prop != 0) {
        icalcomponent_set_dtstart( vevent, start );
    } else {
        prop = icalproperty_new_dtstart( start );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* string GetStartDate (); */
NS_IMETHODIMP oeICalEventImpl::GetStartDate(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetStartDate()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DTSTART_PROPERTY );
    if ( prop != 0) {
        icaltimetype start;
        start = icalproperty_get_dtstart( prop );
        char tmp[80];
        sprintf( tmp, "%d,%d,%d,%d,%d." , start.year, start.month, 
        					     start.day, start.hour, start.minute );
        *aRetVal= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}

NS_IMETHODIMP oeICalEventImpl::SetEndDate( PRInt16 year, PRInt16 month, PRInt16 day, PRInt16 hour, PRInt16 min) {
#ifdef ICAL_DEBUG
    printf( "SetEndDate()\n" );
#endif
    struct icaltimetype end;
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);
    
    end.year = year; end.month = month; end.day = day;
    end.hour = hour; end.minute = min; end.second = 0;
    end.is_utc = false; end.is_date = false;
    icalproperty *dtend = icalproperty_new_dtend( end );

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DTEND_PROPERTY );
    if ( prop != 0) {
        icalcomponent_set_dtend( vevent, end );
    } else {
        prop = icalproperty_new_dtend( end );
        icalcomponent_add_property( vevent, prop );
    }
    return NS_OK;
}

/* string GetEndDate (); */
NS_IMETHODIMP oeICalEventImpl::GetEndDate(char **aRetVal)
{
#ifdef ICAL_DEBUG
    printf( "GetEndDate()\n" );
#endif
    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);

    icalproperty *prop = icalcomponent_get_first_property( vevent, ICAL_DTEND_PROPERTY );
    if ( prop != 0) {
        icaltimetype end;
        end = icalproperty_get_dtstart( prop );
        char tmp[80];
        sprintf( tmp, "%d,%d,%d,%d,%d." , end.year, end.month, 
        					     end.day, end.hour, end.minute );
        *aRetVal= (char*) nsMemory::Clone( tmp, strlen(tmp)+1);
        if( *aRetVal == NULL )
            return  NS_ERROR_OUT_OF_MEMORY;
    } else
        *aRetVal= NULL;
    return NS_OK;
}
/*
NS_IMETHODIMP oeICalEventImpl::SetAlarm( PRInt16 year, PRInt16 month, PRInt16 day, PRInt16 hour, PRInt16 min) {
#ifdef ICAL_DEBUG
    printf( "SetAlarm()\n" );
#endif
    struct icaltriggertype trig;
    struct icaltimetype start;
    icalproperty *prop;

    icalcomponent *vevent = icalcomponent_get_first_component( vcalendar, ICAL_VEVENT_COMPONENT );
    assert( vevent != 0);
    
    start.year = year; start.month = month; start.day = day;
    start.hour = hour; start.minute = min; start.second = 0;
    start.is_utc = false; start.is_date = false;
    trig.time = start;

    icalcomponent *valarm = icalcomponent_get_first_component( vevent, ICAL_VALARM_COMPONENT );
    if( valarm != 0 ) {
        prop = icalcomponent_get_first_property( valarm, ICAL_TRIGGER_PROPERTY );
        if ( prop != 0) {
            icalcomponent_remove_property( valarm, prop );
            icalproperty_free( prop );
        }
    } else {
        valarm = icalcomponent_new_valarm();
    }
    prop = icalproperty_new_trigger( trig );
    icalcomponent_add_property( valarm, prop );
    icalcomponent_add_component( vevent, valarm );
    return NS_OK;
}*/

