/* -*- Mode: javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2006 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Daniel Boelzle (daniel.boelzle@sun.com)
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//
// WCAP error handling helpers
//

const g_wcapErrorCodes = [
    /* -1 */ Components.results.NS_OK, "Logout successful.",
    /*  0 */ Components.results.NS_OK, "Command successful.",
    /*  1 */ Components.interfaces.calIWcapErrors.WCAP_LOGIN_FAILED, "Login failed, session ID timed out. Invalid session ID.",
    /*  2 */ Components.interfaces.calIWcapErrors.WCAP_LOGIN_OK_DEFAULT_CALENDAR_NOT_FOUND, "login.wcap was successful, but the default calendar for this user was not found. A new default calendar set to the userid was created.",
    /*  3 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  4 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  5 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  6 */ Components.interfaces.calIWcapErrors.WCAP_DELETE_EVENTS_BY_ID_FAILED, "Command failed.",
    /*  7 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  8 */ Components.interfaces.calIWcapErrors.WCAP_SETCALPROPS_FAILED, "Command failed.",
    /*  9 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_EVENTS_BY_ID_FAILED, "Command failed.",
    /* 10 */ Components.interfaces.calIWcapErrors.WCAP_CREATECALENDAR_FAILED, "Command failed.",
    /* 11 */ Components.interfaces.calIWcapErrors.WCAP_DELETECALENDAR_FAILED, "Command failed.",
    /* 12 */ Components.interfaces.calIWcapErrors.WCAP_ADDLINK_FAILED, "Command failed.",
    /* 13 */ Components.interfaces.calIWcapErrors.WCAP_FETCHBYDATERANGE_FAILED, "Command failed.",
    /* 14 */ Components.interfaces.calIWcapErrors.WCAP_STOREEVENTS_FAILED, "Command failed.",
    /* 15 */ Components.interfaces.calIWcapErrors.WCAP_STORETODOS_FAILED, "Command failed.",
    /* 16 */ Components.interfaces.calIWcapErrors.WCAP_DELETE_TODOS_BY_ID_FAILED, "Command failed.",
    /* 17 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_TODOS_BY_ID_FAILED, "Command failed.",
    /* 18 */ Components.interfaces.calIWcapErrors.WCAP_FETCHCOMPONENTS_FAILED_BAD_TZID, "Command failed to find correct tzid. Applies to fetchcomponents_by_range.wcap, fetchevents_by_id.wcap, fetchtodos_by_id.wcap.",
    /* 19 */ Components.interfaces.calIWcapErrors.WCAP_SEARCH_CALPROPS_FAILED, "Command failed.",
    /* 20 */ Components.interfaces.calIWcapErrors.WCAP_GET_CALPROPS_FAILED, "Command failed.",
    /* 21 */ Components.interfaces.calIWcapErrors.WCAP_DELETECOMPONENTS_BY_RANGE_FAILED, "Command failed.",
    /* 22 */ Components.interfaces.calIWcapErrors.WCAP_DELETEEVENTS_BY_RANGE_FAILED, "Command failed.",
    /* 23 */ Components.interfaces.calIWcapErrors.WCAP_DELETETODOS_BY_RANGE_FAILED, "Command failed.",
    /* 24 */ Components.interfaces.calIWcapErrors.WCAP_GET_ALL_TIMEZONES_FAILED, "Command failed.",
    /* 25 */ Components.interfaces.calIWcapErrors.WCAP_CREATECALENDAR_ALREADY_EXISTS_FAILED, "The command createcalendar.wcap failed. A calendar with that name already exists in the database.",
    /* 26 */ Components.interfaces.calIWcapErrors.WCAP_SET_USERPREFS_FAILED, "Command failed.",
    /* 27 */ Components.interfaces.calIWcapErrors.WCAP_CHANGE_PASSWORD_FAILED, "Command failed.",
    /* 28 */ Components.interfaces.calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR, "Command failed. The user is denied access to a calendar.",
    /* 29 */ Components.interfaces.calIWcapErrors.WCAP_CALENDAR_DOES_NOT_EXIST, "Command failed. The requested calendar does not exist in the database.",
    /* 30 */ Components.interfaces.calIWcapErrors.WCAP_ILLEGAL_CALID_NAME, "createcalendar.wcap failed. Invalid calid passed in.",
    /* 31 */ Components.interfaces.calIWcapErrors.WCAP_CANNOT_MODIFY_LINKED_EVENTS, "storeevents.wcap failed. The event to modify was a linked event.",
    /* 32 */ Components.interfaces.calIWcapErrors.WCAP_CANNOT_MODIFY_LINKED_TODOS, "storetodos.wcap failed. The todo to modify was a linked todo.",
    /* 33 */ Components.interfaces.calIWcapErrors.WCAP_CANNOT_SENT_EMAIL, "Command failed. Email notification failed. Usually caused by the server not being properly configured to send email. This can occur in storeevents.wcap, storetodos.wcap, deleteevents_by_id.wcap, deletetodos_by_id.wcap.",
    /* 34 */ Components.interfaces.calIWcapErrors.WCAP_CALENDAR_DISABLED, "Command failed. The calendar is disabled in the database.",
    /* 35 */ Components.interfaces.calIWcapErrors.WCAP_WRITE_IMPORT_FAILED, "Import failed when writing files to the server.",
    /* 36 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_BY_LAST_MODIFIED_FAILED, "Command failed.",
    /* 37 */ Components.interfaces.calIWcapErrors.WCAP_CAPI_NOT_SUPPORTED, "Failed trying to read from unsupported format calendar data.",
    /* 38 */ Components.interfaces.calIWcapErrors.WCAP_CALID_NOT_SPECIFIED, "Calendar ID was not specified.",
    /* 39 */ Components.interfaces.calIWcapErrors.WCAP_GET_FREEBUSY_FAILED, "Command failed.",
    /* 40 */ Components.interfaces.calIWcapErrors.WCAP_STORE_FAILED_DOUBLE_BOOKED, "If double booking is not allowed in this calendar, storeevents.wcap fails with this error when attempting to store an event in a time slot that was already filled.",
    /* 41 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_BY_ALARM_RANGE_FAILED, "Command failed.",
    /* 42 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_BY_ATTENDEE_ERROR_FAILED, "Command failed.",
    /* 43 */ Components.interfaces.calIWcapErrors.WCAP_ATTENDEE_GROUP_EXPANSION_CLIPPED, "An LDAP group being expanded was too large and exceeded the maximum number allowed in an expansion. The expansion stopped at the specified maximum limit. The maximum limit defaults to 200. To change the maximum limit, set the server configuration preference calstore.group.attendee.maxsize.",
    /* 44 */ Components.interfaces.calIWcapErrors.WCAP_USERPREFS_ACCESS_DENIED, "Either the server does not allow this administrator access to get or modify user preferences, or the requester is not an administrator.",
    /* 45 */ Components.interfaces.calIWcapErrors.WCAP_NOT_ALLOWED_TO_REQUEST_PUBLISH, "The requester was not an organizer of the event, and, therefore, is not allowed to edit the component using the PUBLISH or REQUEST method.",
    /* 46 */ Components.interfaces.calIWcapErrors.WCAP_INSUFFICIENT_PARAMETERS, "The caller tried to invoke verifyevents_by_ids.wcap, or verifytodos_by_ids.wcap with insufficient arguments (mismatched number of uid’s and rid’s).",
    /* 47 */ Components.interfaces.calIWcapErrors.WCAP_MUSTBEOWNER_OPERATION, "The user needs to be an owner or co-owner of the calendar in questions to complete this operation. (Probably related to private or confidential component.)",
    /* 48 */ Components.interfaces.calIWcapErrors.WCAP_DWP_CONNECTION_FAILED, "GSE scheduling engine failed to make connection to DWP.",
    /* 49 */ Components.interfaces.calIWcapErrors.WCAP_DWP_MAX_CONNECTION_REACHED, "Reached the maximum number of connections. When some of the connections are freed, users can successfully connect. Same as error 11001.",
    /* 50 */ Components.interfaces.calIWcapErrors.WCAP_DWP_CANNOT_RESOLVE_CALENDAR, "Front end can’t resolve to a particular back end. Same as error 11002.",
    /* 51 */ Components.interfaces.calIWcapErrors.WCAP_DWP_BAD_DATA, "Generic response. Check all DWP servers. One might be down. Same as error 11003.",
    /* 52 */ Components.interfaces.calIWcapErrors.WCAP_BAD_COMMAND, "The command sent in was not recognized. This is an internal only error code. It should not appear in the error logs.",
    /* 53 */ Components.interfaces.calIWcapErrors.WCAP_NOT_FOUND, "Returned for all errors from a write to the Berkeley DB. This is an internal only error code. It should not appear in the error logs.",
    /* 54 */ Components.interfaces.calIWcapErrors.WCAP_WRITE_IMPORT_CANT_EXPAND_CALID, "Can’t expand calid when importing file.",
    /* 55 */ Components.interfaces.calIWcapErrors.WCAP_GETTIME_FAILED, "Get server time failed.",
    /* 56 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_DELETEDCOMPONENTS_FAILED, "fetch_deletedcomponents.wcap failed.",
    /* 57 */ Components.interfaces.calIWcapErrors.WCAP_FETCH_DELETEDCOMPONENTS_PARTIAL_RESULT, "Success but partial result.",
    /* 58 */ Components.interfaces.calIWcapErrors.WCAP_WCAP_NO_SUCH_FORMAT, "Returned in any of the commands when supplied fmt-out is not a supported format.",
    /* 59 */ Components.interfaces.calIWcapErrors.WCAP_COMPONENT_NOT_FOUND, "Returned when a fetch or delete is attempted that does not exist.",
    /* 60 */ Components.interfaces.calIWcapErrors.WCAP_BAD_ARGUMENTS, "Currently used when attendee or organizer specified does not have a valid email address.",
    /* 61 */ Components.interfaces.calIWcapErrors.WCAP_GET_USERPREFS_FAILED, "get_userprefs.wcap failed. The following error conditions returns error code 61: LDAP access denied, no results found, LDAP limit exceeded, LDAP connection failed.",
    /* 62 */ Components.interfaces.calIWcapErrors.WCAP_WCAP_MODIFY_NO_EVENT, "storeevents.wcap issued with storeytype set to 2 (WCAP_STORE_TYPE_MODIFY) and the event doesn\’t exist.",
    /* 63 */ Components.interfaces.calIWcapErrors.WCAP_WCAP_CREATE_EXISTS, "storeevents.wcap issued with storetype set to 1 (WCAP_STORE_TYPE_CREATE) and the event already exists.",
    /* 64 */ Components.interfaces.calIWcapErrors.WCAP_WCAP_MODIFY_CANT_MAKE_COPY, "storevents.wcap issued and copy of event failed during processing.",
    /* 65 */ Components.interfaces.calIWcapErrors.WCAP_STORE_FAILED_RECUR_SKIP, "One instance of a recurring event skips over another.",
    /* 66 */ Components.interfaces.calIWcapErrors.WCAP_STORE_FAILED_RECUR_SAMEDAY, "Two instances of a recurring event can’t occur on the same day.",
    /* 67 */ Components.interfaces.calIWcapErrors.WCAP_BAD_ORG_ARGUMENTS, "Bad organizer arguments. orgCalid or orgEmail must be passed if any other \"org\" parameter is sent. That is, orgUID can’t be sent alone on a storeevents.wcap or a storetodos.wcao command if it is trying about to \"create\" the event or task. Note, if no \"org\" information is passed, the organizer defaults to the calid being passed with the command.",
    /* 68 */ Components.interfaces.calIWcapErrors.WCAP_STORE_FAILED_RECUR_PRIVACY, "Error returned if you try to change the privacy or transparency of a single instance in a recurring series.",
    /* 69 */ Components.interfaces.calIWcapErrors.WCAP_LDAP_ERROR, "For get_calprops.wcap, when there is an error is getting LDAP derived token values (X-S1CS-CALPROPS-FB-INCLUDE, X-S1CS-CALPROPS-COMMON-NAME).",
    /* 70 */ Components.interfaces.calIWcapErrors.WCAP_GET_INVITE_COUNT_FAILED, "Error in getting invite count (for get_calprops.wcap and fetchcomponents_by_range.wcap commands).",
    /* 71 */ Components.interfaces.calIWcapErrors.WCAP_LIST_FAILED, "list.wcap failed.",
    /* 72 */ Components.interfaces.calIWcapErrors.WCAP_LIST_SUBSCRIBED_FAILED, "list_subscribed.wcap failed.",
    /* 73 */ Components.interfaces.calIWcapErrors.WCAP_SUBSCRIBE_FAILED, "subscribe.wcap failed.",
    /* 74 */ Components.interfaces.calIWcapErrors.WCAP_UNSUBSCRIBE_FAILED, "unsubscribe.wcap failed.",
    /* 75 */ Components.interfaces.calIWcapErrors.WCAP_ANONYMOUS_NOT_ALLOWED, "Command cannot be executed as anonymous. Used only for list.wcap, list_subscribed.wcap, subscribe.wcap, and unsubscribe.wcap commands.",
    /* 76 */ Components.interfaces.calIWcapErrors.WCAP_ACCESS_DENIED, "Generated if a non-administrator user tries to read or set the calendar-owned list or the calendar-subscribed list of some other user, or if the option is not turned on in the server.",
    /* 77 */ Components.interfaces.calIWcapErrors.WCAP_BAD_IMPORT_ARGUMENTS, "Incorrect parameter received by import.wcap.",
    /* 78 */ Components.interfaces.calIWcapErrors.WCAP_READONLY_DATABASE, "Database is in read-only mode (returned for all attempts to write to the database).",
    /* 79 */ Components.interfaces.calIWcapErrors.WCAP_ATTENDEE_NOT_ALLOWED_TO_REQUEST_ON_MODIFY, "Attendee is not allowed to modify an event with method=request.",
    /* 80 */ Components.interfaces.calIWcapErrors.WCAP_TRANSP_RESOURCE_NOT_ALLOWED, "Resources do not permit the transparency parameter.",
    /* 81 */ Components.interfaces.calIWcapErrors.WCAP_RECURRING_COMPONENT_NOT_FOUND, "Recurring component not found. Only happens when recurring=1 is passed in by fetch commands. This code is returned if part of the recurring series (either the master or an exception) is missing.",
    /* 82 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 83 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 84 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 85 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 86 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 87 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 88 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 89 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 90 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 91 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 92 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 93 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 94 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 95 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 96 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 97 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 98 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 99 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /* 11000 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_MAX_CONNECTION_REACHED, "Maximum connections to back-end database reached. As connections are freed up, users can connect to the back-end.",
    /* 11001 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_CANNOT_CONNECT, "Cannot connect to back-end server. Back-end machine might be down or DWP server is not up and running.",
    /* 11002 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_CANNOT_RESOLVE_CALENDAR, "Front-end can’t resolve calendar to a particular back-end server.",
    /* 11003 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_BAD_DATA, "Bad data received from DWP connection. This is a generic formatting error. Check all DWP servers. One might be down.",
    /* 11004 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_DWPHOST_CTX_DOES_NOT_EXIST, "For the back-end host, context doesn\’t exist in the context table.",
    /* 11005 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_HOSTNAME_NOT_RESOLVABLE, "DNS or NIS files, or hostname resolver is not set up properly or machine does not exist.",
    /* 11006 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_NO_DATA, "No data was received from reading the calendar properties from the DWP connection.",
    /* 11007 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_AUTH_FAILED, "DWP authentication failed.",
    /* 11008 */ Components.interfaces.calIWcapErrors.WCAP_CDWP_ERR_CHECKVERSION_FAILED, "DWP version check failed."
    ];

function getWcapErrorIndex( errno )
{
    var index = -1;
    if (errno >= -1 && errno <= 81)
        index = (errno + 1);
    else if (errno >= 11000 && errno <= 11008)
        index = (errno - 11000 + 100 + 1);
    if (index >= 0 &&
        g_wcapErrorCodes[index * 2] != Components.results.NS_ERROR_INVALID_ARG)
    {
        return index;
    }
    else
        throw Components.results.NS_ERROR_INVALID_ARG;
}

function getWcapErrorIndexByErrorCode( rc )
{
    var index = (rc - Components.interfaces.calIWcapErrors.WCAP_ERROR_BASE + 1);
    if (index >= 1 && index <= 108 &&
        g_wcapErrorCodes[index * 2] != Components.results.NS_ERROR_INVALID_ARG)
    {
        return index;
    }
    else
        throw Components.results.NS_ERROR_INVALID_ARG;
}

function getWcapErrorCodeString( rc )
{
    return g_wcapErrorCodes[(getWcapErrorIndexByErrorCode(rc) * 2) + 1];
}

function getWcapErrorCode( errno )
{
    return g_wcapErrorCodes[getWcapErrorIndex(errno) * 2];
}

function getWcapXmlErrno( xml )
{
    if (xml == undefined)
        throw new Error("no XML!");
    var item = xml.getElementsByTagName("X-NSCP-WCAP-ERRNO").item(0);
    if (item)
        return parseInt(item.textContent);
    else {
        // xxx todo: throw new Eror("missing element X-NSCP-WCAP-ERRNO!");
        //           cs currently may forget to send X-NSCP-WCAP-ERRNO on
        //           some commands, maybe fixed in later versions. WTF.
        return 0;
    }
}

function getWcapIcalErrno( icalRootComp )
{
    if (icalRootComp == undefined)
        throw new Error("no VCALENDAR root component!");
    var prop = icalRootComp.getFirstProperty( "X-NSCP-WCAP-ERRNO" );
    if (prop)
        return parseInt(prop.value);
    else {
        // xxx todo: throw new Eror("missing element X-NSCP-WCAP-ERRNO!");
        //           cs currently may forget to send X-NSCP-WCAP-ERRNO on
        //           some commands, maybe fixed in later versions. WTF.
        return 0;
    }
}

function checkWcapErrno( errno, expectedErrno )
{
    if (expectedErrno == undefined) {
        expectedErrno = 0;
    }
    if (errno != expectedErrno) {
        throw getWcapErrorCode(errno);
    }
}

function checkWcapXmlErrno( xml, expectedErrno )
{
    checkWcapErrno( getWcapXmlErrno(xml), expectedErrno );
}

function checkWcapIcalErrno( icalRootComp, expectedErrno )
{
    checkWcapErrno( getWcapIcalErrno(icalRootComp), expectedErrno );
}

