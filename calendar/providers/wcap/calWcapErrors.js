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
// Common netwerk errors:
//
const NS_ERROR_MODULE_BASE_OFFSET = 0x45;
const NS_ERROR_MODULE_NETWORK = 6;

function generateFailure(module, code) {
    return ((1<<31) | ((module + NS_ERROR_MODULE_BASE_OFFSET) << 16) | code);
}

function generateNetFailure(code) {
    return generateFailure(NS_ERROR_MODULE_NETWORK, code);
}

function getErrorModule(rc) {
    return (((rc >>> 16) & 0x7fff) - NS_ERROR_MODULE_BASE_OFFSET);
}

// Cannot perform operation, because user is offline.
// The following error codes have been adopted from
// netwerk/base/public/nsNetError.h
// and ought to be defined in IDL. xxx todo

// The requested action could not be completed while the networking
// is in the offline state.
const NS_ERROR_OFFLINE = generateNetFailure(16);

// The async request completed successfully.
const NS_BINDING_SUCCEEDED = NS_OK;

const NS_BINDING_FAILED = generateNetFailure(1);
const NS_BINDING_ABORTED = generateNetFailure(2);
const NS_BINDING_REDIRECTED = generateNetFailure(3);
const NS_BINDING_RETARGETED = generateNetFailure(4);

const g_nsNetErrorCodes = [
    NS_BINDING_FAILED,
    "The async request failed for some unknown reason.",
    NS_BINDING_ABORTED,
    "The async request failed because it was aborted by some user action.",
    NS_BINDING_REDIRECTED,
    "The async request has been redirected to a different async request.",
    NS_BINDING_RETARGETED,
    "The async request has been retargeted to a different handler.",
    /*NS_ERROR_MALFORMED_URI*/ generateNetFailure(10),
    "The URI is malformed.",
    /*NS_ERROR_UNKNOWN_PROTOCOL*/ generateNetFailure(18),
    "The URI scheme corresponds to an unknown protocol handler.",
    /*NS_ERROR_CONNECTION_REFUSED*/ generateNetFailure(13),
    "The connection attempt failed, for example, because no server was listening at specified host:port.",
    /*NS_ERROR_PROXY_CONNECTION_REFUSED*/ generateNetFailure(72),
    "The connection attempt to a proxy failed.",
    /*NS_ERROR_NET_TIMEOUT*/ generateNetFailure(14),
    "The connection was lost due to a timeout error.",
    NS_ERROR_OFFLINE,
    "The requested action could not be completed while the networking library is in the offline state.",
    /*NS_ERROR_PORT_ACCESS_NOT_ALLOWED*/ generateNetFailure(19),
    "The requested action was prohibited because it would have caused the networking library to establish a connection to an unsafe or otherwise banned port.",
    /*NS_ERROR_NET_RESET*/ generateNetFailure(20),
    "The connection was established, but no data was ever received.",
    /*NS_ERROR_NET_INTERRUPT*/ generateNetFailure(71),
    "The connection was established, but the data transfer was interrupted.",
    /*NS_ERROR_NOT_RESUMABLE*/ generateNetFailure(25),
    "This request is not resumable, but it was tried to resume it, or to request resume-specific data.",
    /*NS_ERROR_ENTITY_CHANGED*/ generateNetFailure(32),
    "It was attempted to resume the request, but the entity has changed in the meantime.",
    /*NS_ERROR_REDIRECT_LOOP*/ generateNetFailure(31),
    "The request failed as a result of a detected redirection loop.",
    /*NS_ERROR_UNKNOWN_HOST*/ generateNetFailure(30),
    "The lookup of a hostname failed. This generally refers to the hostname from the URL being loaded.",
    /*NS_ERROR_UNKNOWN_PROXY_HOST*/ generateNetFailure(42),
    "The lookup of a proxy hostname failed.",
    /*NS_ERROR_UNKNOWN_SOCKET_TYPE*/ generateNetFailure(51),
    "The specified socket type does not exist.",
    /*NS_ERROR_SOCKET_CREATE_FAILED*/ generateNetFailure(52),
    "The specified socket type could not be created."
    ];

function netErrorToString( rc )
{
    if (!isNaN(rc) && getErrorModule(rc) == NS_ERROR_MODULE_NETWORK) {
        var i = 0;
        while (i < g_nsNetErrorCodes.length) {
            // however rc is kept unsigned, our generated code signed,
            // so == won't work here:
            if ((g_nsNetErrorCodes[i] ^ rc) == 0)
                return g_nsNetErrorCodes[i + 1];
            i += 2;
        }
    }
    throw Components.results.NS_ERROR_INVALID_ARG;
}


//
// WCAP error handling helpers
//

const g_wcapErrorCodes = [
    /* -1 */ NS_OK, "Logout successful.",
    /*  0 */ NS_OK, "Command successful.",
    /*  1 */ calIWcapErrors.WCAP_LOGIN_FAILED, "Login failed. Invalid session ID.",
    /*  2 */ calIWcapErrors.WCAP_LOGIN_OK_DEFAULT_CALENDAR_NOT_FOUND, "login.wcap was successful, but the default calendar for this user was not found. A new default calendar set to the userid was created.",
    /*  3 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  4 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  5 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  6 */ calIWcapErrors.WCAP_DELETE_EVENTS_BY_ID_FAILED, "WCAP_DELETE_EVENTS_BY_ID_FAILED",
    /*  7 */ Components.results.NS_ERROR_INVALID_ARG, "No WCAP error code.",
    /*  8 */ calIWcapErrors.WCAP_SETCALPROPS_FAILED, "WCAP_SETCALPROPS_FAILED",
    /*  9 */ calIWcapErrors.WCAP_FETCH_EVENTS_BY_ID_FAILED, "WCAP_FETCH_EVENTS_BY_ID_FAILED",
    /* 10 */ calIWcapErrors.WCAP_CREATECALENDAR_FAILED, "WCAP_CREATECALENDAR_FAILED",
    /* 11 */ calIWcapErrors.WCAP_DELETECALENDAR_FAILED, "WCAP_DELETECALENDAR_FAILED",
    /* 12 */ calIWcapErrors.WCAP_ADDLINK_FAILED, "WCAP_ADDLINK_FAILED",
    /* 13 */ calIWcapErrors.WCAP_FETCHBYDATERANGE_FAILED, "WCAP_FETCHBYDATERANGE_FAILED",
    /* 14 */ calIWcapErrors.WCAP_STOREEVENTS_FAILED, "WCAP_STOREEVENTS_FAILED",
    /* 15 */ calIWcapErrors.WCAP_STORETODOS_FAILED, "WCAP_STORETODOS_FAILED",
    /* 16 */ calIWcapErrors.WCAP_DELETE_TODOS_BY_ID_FAILED, "WCAP_DELETE_TODOS_BY_ID_FAILED",
    /* 17 */ calIWcapErrors.WCAP_FETCH_TODOS_BY_ID_FAILED, "WCAP_FETCH_TODOS_BY_ID_FAILED",
    /* 18 */ calIWcapErrors.WCAP_FETCHCOMPONENTS_FAILED_BAD_TZID, "Command failed to find correct tzid. Applies to fetchcomponents_by_range.wcap, fetchevents_by_id.wcap, fetchtodos_by_id.wcap.",
    /* 19 */ calIWcapErrors.WCAP_SEARCH_CALPROPS_FAILED, "WCAP_SEARCH_CALPROPS_FAILED",
    /* 20 */ calIWcapErrors.WCAP_GET_CALPROPS_FAILED, "WCAP_GET_CALPROPS_FAILED",
    /* 21 */ calIWcapErrors.WCAP_DELETECOMPONENTS_BY_RANGE_FAILED, "WCAP_DELETECOMPONENTS_BY_RANGE_FAILED",
    /* 22 */ calIWcapErrors.WCAP_DELETEEVENTS_BY_RANGE_FAILED, "WCAP_DELETEEVENTS_BY_RANGE_FAILED",
    /* 23 */ calIWcapErrors.WCAP_DELETETODOS_BY_RANGE_FAILED, "WCAP_DELETETODOS_BY_RANGE_FAILED",
    /* 24 */ calIWcapErrors.WCAP_GET_ALL_TIMEZONES_FAILED, "WCAP_GET_ALL_TIMEZONES_FAILED",
    /* 25 */ calIWcapErrors.WCAP_CREATECALENDAR_ALREADY_EXISTS_FAILED, "The command createcalendar.wcap failed. A calendar with that name already exists in the database.",
    /* 26 */ calIWcapErrors.WCAP_SET_USERPREFS_FAILED, "WCAP_SET_USERPREFS_FAILED",
    /* 27 */ calIWcapErrors.WCAP_CHANGE_PASSWORD_FAILED, "WCAP_CHANGE_PASSWORD_FAILED",
    /* 28 */ calIWcapErrors.WCAP_ACCESS_DENIED_TO_CALENDAR, "Command failed. The user is denied access to a calendar.",
    /* 29 */ calIWcapErrors.WCAP_CALENDAR_DOES_NOT_EXIST, "Command failed. The requested calendar does not exist in the database.",
    /* 30 */ calIWcapErrors.WCAP_ILLEGAL_CALID_NAME, "createcalendar.wcap failed. Invalid calid passed in.",
    /* 31 */ calIWcapErrors.WCAP_CANNOT_MODIFY_LINKED_EVENTS, "storeevents.wcap failed. The event to modify was a linked event.",
    /* 32 */ calIWcapErrors.WCAP_CANNOT_MODIFY_LINKED_TODOS, "storetodos.wcap failed. The todo to modify was a linked todo.",
    /* 33 */ calIWcapErrors.WCAP_CANNOT_SENT_EMAIL, "Command failed. Email notification failed. Usually caused by the server not being properly configured to send email. This can occur in storeevents.wcap, storetodos.wcap, deleteevents_by_id.wcap, deletetodos_by_id.wcap.",
    /* 34 */ calIWcapErrors.WCAP_CALENDAR_DISABLED, "Command failed. The calendar is disabled in the database.",
    /* 35 */ calIWcapErrors.WCAP_WRITE_IMPORT_FAILED, "Import failed when writing files to the server.",
    /* 36 */ calIWcapErrors.WCAP_FETCH_BY_LAST_MODIFIED_FAILED, "WCAP_FETCH_BY_LAST_MODIFIED_FAILED",
    /* 37 */ calIWcapErrors.WCAP_CAPI_NOT_SUPPORTED, "Failed trying to read from unsupported format calendar data.",
    /* 38 */ calIWcapErrors.WCAP_CALID_NOT_SPECIFIED, "Calendar ID was not specified.",
    /* 39 */ calIWcapErrors.WCAP_GET_FREEBUSY_FAILED, "WCAP_GET_FREEBUSY_FAILED",
    /* 40 */ calIWcapErrors.WCAP_STORE_FAILED_DOUBLE_BOOKED, "If double booking is not allowed in this calendar, storeevents.wcap fails with this error when attempting to store an event in a time slot that was already filled.",
    /* 41 */ calIWcapErrors.WCAP_FETCH_BY_ALARM_RANGE_FAILED, "WCAP_FETCH_BY_ALARM_RANGE_FAILED",
    /* 42 */ calIWcapErrors.WCAP_FETCH_BY_ATTENDEE_ERROR_FAILED, "WCAP_FETCH_BY_ATTENDEE_ERROR_FAILED",
    /* 43 */ calIWcapErrors.WCAP_ATTENDEE_GROUP_EXPANSION_CLIPPED, "An LDAP group being expanded was too large and exceeded the maximum number allowed in an expansion. The expansion stopped at the specified maximum limit. The maximum limit defaults to 200. To change the maximum limit, set the server configuration preference calstore.group.attendee.maxsize.",
    /* 44 */ calIWcapErrors.WCAP_USERPREFS_ACCESS_DENIED, "Either the server does not allow this administrator access to get or modify user preferences, or the requester is not an administrator.",
    /* 45 */ calIWcapErrors.WCAP_NOT_ALLOWED_TO_REQUEST_PUBLISH, "The requester was not an organizer of the event, and, therefore, is not allowed to edit the component using the PUBLISH or REQUEST method.",
    /* 46 */ calIWcapErrors.WCAP_INSUFFICIENT_PARAMETERS, "The caller tried to invoke verifyevents_by_ids.wcap, or verifytodos_by_ids.wcap with insufficient arguments (mismatched number of uid’s and rid’s).",
    /* 47 */ calIWcapErrors.WCAP_MUSTBEOWNER_OPERATION, "The user needs to be an owner or co-owner of the calendar in questions to complete this operation. (Probably related to private or confidential component.)",
    /* 48 */ calIWcapErrors.WCAP_DWP_CONNECTION_FAILED, "GSE scheduling engine failed to make connection to DWP.",
    /* 49 */ calIWcapErrors.WCAP_DWP_MAX_CONNECTION_REACHED, "Reached the maximum number of connections. When some of the connections are freed, users can successfully connect. Same as error 11001.",
    /* 50 */ calIWcapErrors.WCAP_DWP_CANNOT_RESOLVE_CALENDAR, "Front end can’t resolve to a particular back end. Same as error 11002.",
    /* 51 */ calIWcapErrors.WCAP_DWP_BAD_DATA, "Generic response. Check all DWP servers. One might be down. Same as error 11003.",
    /* 52 */ calIWcapErrors.WCAP_BAD_COMMAND, "The command sent in was not recognized. This is an internal only error code. It should not appear in the error logs.",
    /* 53 */ calIWcapErrors.WCAP_NOT_FOUND, "Returned for all errors from a write to the Berkeley DB. This is an internal only error code. It should not appear in the error logs.",
    /* 54 */ calIWcapErrors.WCAP_WRITE_IMPORT_CANT_EXPAND_CALID, "Can’t expand calid when importing file.",
    /* 55 */ calIWcapErrors.WCAP_GETTIME_FAILED, "Get server time failed.",
    /* 56 */ calIWcapErrors.WCAP_FETCH_DELETEDCOMPONENTS_FAILED, "fetch_deletedcomponents.wcap failed.",
    /* 57 */ calIWcapErrors.WCAP_FETCH_DELETEDCOMPONENTS_PARTIAL_RESULT, "Success but partial result.",
    /* 58 */ calIWcapErrors.WCAP_WCAP_NO_SUCH_FORMAT, "Returned in any of the commands when supplied fmt-out is not a supported format.",
    /* 59 */ calIWcapErrors.WCAP_COMPONENT_NOT_FOUND, "Returned when a fetch or delete is attempted that does not exist.",
    /* 60 */ calIWcapErrors.WCAP_BAD_ARGUMENTS, "Currently used when attendee or organizer specified does not have a valid email address.",
    /* 61 */ calIWcapErrors.WCAP_GET_USERPREFS_FAILED, "get_userprefs.wcap failed. The following error conditions returns error code 61: LDAP access denied, no results found, LDAP limit exceeded, LDAP connection failed.",
    /* 62 */ calIWcapErrors.WCAP_WCAP_MODIFY_NO_EVENT, "storeevents.wcap issued with storetype set to 2 (WCAP_STORE_TYPE_MODIFY) and the event doesn\’t exist.",
    /* 63 */ calIWcapErrors.WCAP_WCAP_CREATE_EXISTS, "storeevents.wcap issued with storetype set to 1 (WCAP_STORE_TYPE_CREATE) and the event already exists.",
    /* 64 */ calIWcapErrors.WCAP_WCAP_MODIFY_CANT_MAKE_COPY, "storevents.wcap issued and copy of event failed during processing.",
    /* 65 */ calIWcapErrors.WCAP_STORE_FAILED_RECUR_SKIP, "One instance of a recurring event skips over another.",
    /* 66 */ calIWcapErrors.WCAP_STORE_FAILED_RECUR_SAMEDAY, "Two instances of a recurring event can’t occur on the same day.",
    /* 67 */ calIWcapErrors.WCAP_BAD_ORG_ARGUMENTS, "Bad organizer arguments. orgCalid or orgEmail must be passed if any other \"org\" parameter is sent. That is, orgUID can’t be sent alone on a storeevents.wcap or a storetodos.wcao command if it is trying about to \"create\" the event or task. Note, if no \"org\" information is passed, the organizer defaults to the calid being passed with the command.",
    /* 68 */ calIWcapErrors.WCAP_STORE_FAILED_RECUR_PRIVACY, "Error returned if you try to change the privacy or transparency of a single instance in a recurring series.",
    /* 69 */ calIWcapErrors.WCAP_LDAP_ERROR, "For get_calprops.wcap, when there is an error is getting LDAP derived token values (X-S1CS-CALPROPS-FB-INCLUDE, X-S1CS-CALPROPS-COMMON-NAME).",
    /* 70 */ calIWcapErrors.WCAP_GET_INVITE_COUNT_FAILED, "Error in getting invite count (for get_calprops.wcap and fetchcomponents_by_range.wcap commands).",
    /* 71 */ calIWcapErrors.WCAP_LIST_FAILED, "list.wcap failed.",
    /* 72 */ calIWcapErrors.WCAP_LIST_SUBSCRIBED_FAILED, "list_subscribed.wcap failed.",
    /* 73 */ calIWcapErrors.WCAP_SUBSCRIBE_FAILED, "subscribe.wcap failed.",
    /* 74 */ calIWcapErrors.WCAP_UNSUBSCRIBE_FAILED, "unsubscribe.wcap failed.",
    /* 75 */ calIWcapErrors.WCAP_ANONYMOUS_NOT_ALLOWED, "Command cannot be executed as anonymous. Used only for list.wcap, list_subscribed.wcap, subscribe.wcap, and unsubscribe.wcap commands.",
    /* 76 */ calIWcapErrors.WCAP_ACCESS_DENIED, "Generated if a non-administrator user tries to read or set the calendar-owned list or the calendar-subscribed list of some other user, or if the option is not turned on in the server.",
    /* 77 */ calIWcapErrors.WCAP_BAD_IMPORT_ARGUMENTS, "Incorrect parameter received by import.wcap.",
    /* 78 */ calIWcapErrors.WCAP_READONLY_DATABASE, "Database is in read-only mode (returned for all attempts to write to the database).",
    /* 79 */ calIWcapErrors.WCAP_ATTENDEE_NOT_ALLOWED_TO_REQUEST_ON_MODIFY, "Attendee is not allowed to modify an event with method=request.",
    /* 80 */ calIWcapErrors.WCAP_TRANSP_RESOURCE_NOT_ALLOWED, "Resources do not permit the transparency parameter.",
    /* 81 */ calIWcapErrors.WCAP_RECURRING_COMPONENT_NOT_FOUND, "Recurring component not found. Only happens when recurring=1 is passed in by fetch commands. This code is returned if part of the recurring series (either the master or an exception) is missing.",
    /* new by WCAP 4.0: */
    /* 82 */ calIWcapErrors.WCAP_BAD_MIME_TYPE,
    "The mime headers supplied while storing the attachment using storeevents.wcap/storetodos.wcap is malformatted.",
    /* 83 */ calIWcapErrors.WCAP_MISSING_BOUNDARY,
    "While supplying attachments to the storeveents/storetodos commands the mime boundary was not found.",
    /* 84 */ calIWcapErrors.WCAP_INVALID_ATTACHMENT,
    "The attachment supplied to be stored on the server is malformatted.",
    /* 85 */ calIWcapErrors.WCAP_ATTACH_DELETE_SUCCESS,
    "All the attachments requested to be deleted from the server by supplying deleteattach were deleted successsfully.",
    /* 86 */ calIWcapErrors.WCAP_ATTACH_DELETE_PARTIAL,
    "Of All attachments requested to be deleted from the server by supplying deleteattach , only few were deleted successfully.",
    /* 87 */ calIWcapErrors.WCAP_ATTACHMENT_NOT_FOUND,
    "The attachent requested to be fetched or deleted from the server was not found.",
    /* / new by WCAP 4.0 */
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
    /* 11000 */ calIWcapErrors.WCAP_CDWP_ERR_MAX_CONNECTION_REACHED, "Maximum connections to back-end database reached. As connections are freed up, users can connect to the back-end.",
    /* 11001 */ calIWcapErrors.WCAP_CDWP_ERR_CANNOT_CONNECT, "Cannot connect to back-end server. Back-end machine might be down or DWP server is not up and running.",
    /* 11002 */ calIWcapErrors.WCAP_CDWP_ERR_CANNOT_RESOLVE_CALENDAR, "Front-end can’t resolve calendar to a particular back-end server.",
    /* 11003 */ calIWcapErrors.WCAP_CDWP_ERR_BAD_DATA, "Bad data received from DWP connection. This is a generic formatting error. Check all DWP servers. One might be down.",
    /* 11004 */ calIWcapErrors.WCAP_CDWP_ERR_DWPHOST_CTX_DOES_NOT_EXIST, "For the back-end host, context doesn\’t exist in the context table.",
    /* 11005 */ calIWcapErrors.WCAP_CDWP_ERR_HOSTNAME_NOT_RESOLVABLE, "DNS or NIS files, or hostname resolver is not set up properly or machine does not exist.",
    /* 11006 */ calIWcapErrors.WCAP_CDWP_ERR_NO_DATA, "No data was received from reading the calendar properties from the DWP connection.",
    /* 11007 */ calIWcapErrors.WCAP_CDWP_ERR_AUTH_FAILED, "DWP authentication failed.",
    /* 11008 */ calIWcapErrors.WCAP_CDWP_ERR_CHECKVERSION_FAILED, "DWP version check failed."
    ];

function wcapErrorToString( rc )
{
    if (isNaN(rc))
        throw Components.results.NS_ERROR_INVALID_ARG;
    if (rc == calIWcapErrors.WCAP_NO_ERRNO)
        return "No WCAP errno (missing X-NSCP-WCAP-ERRNO).";
    
    var index = (rc - calIWcapErrors.WCAP_ERROR_BASE + 1);
    if (index >= 1 && index <= 108 &&
        g_wcapErrorCodes[index * 2] != Components.results.NS_ERROR_INVALID_ARG)
    {
        return g_wcapErrorCodes[(index * 2) + 1];
    }
    throw Components.results.NS_ERROR_INVALID_ARG;
}

function getWcapErrorCode(errno)
{
    var index = -1;
    if (errno >= -1 && errno <= 81)
        index = (errno + 1);
    else if (errno >= 11000 && errno <= 11008)
        index = (errno - 11000 + 100 + 1);
    if (index >= 0 &&
        g_wcapErrorCodes[index * 2] != Components.results.NS_ERROR_INVALID_ARG)
    {
        return g_wcapErrorCodes[index * 2];
    }
    throw Components.results.NS_ERROR_INVALID_ARG;
}

function getWcapXmlErrno(xml)
{
    var elem = xml.getElementsByTagName("X-NSCP-WCAP-ERRNO");
    if (elem) {
        elem = elem.item(0);
        if (elem)
            return parseInt(elem.textContent);
    }
    // some commands just respond with an empty calendar, no errno. WTF.
    // assume success:
    return undefined;
}

function getWcapIcalErrno(icalRootComp)
{
    var prop = icalRootComp.getFirstProperty("X-NSCP-WCAP-ERRNO");
    if (prop)
        return parseInt(prop.value);
    // some commands just respond with an empty calendar, no errno. WTF.
    // assume success:
    return undefined;
}

function checkWcapErrno(errno, expectedErrno)
{
    if (expectedErrno === undefined)
        expectedErrno = 0; // i.e. Command successful.
    if (errno !== undefined && errno != expectedErrno) {
        var rc;
        try {
            rc = getWcapErrorCode(errno);
        }
        catch (exc) {
            throw new Components.Exception(
                "No WCAP error no.", Components.results.NS_ERROR_INVALID_ARG);
        }
        throw new Components.Exception(wcapErrorToString(rc), rc);
    }
}

function checkWcapXmlErrno(xml, expectedErrno) {
    checkWcapErrno(getWcapXmlErrno(xml), expectedErrno);
}

function checkWcapIcalErrno(icalRootComp, expectedErrno) {
    checkWcapErrno(getWcapIcalErrno(icalRootComp), expectedErrno);
}

function getResultCode(err)
{
    if (err === undefined || err === null)
        return NS_OK;
    if (isNaN(err)) {
        if (err instanceof nsIException)
            return err.result;
        else
            return Components.results.NS_ERROR_FAILURE;
    }
    return err;
}

function errorToString(err)
{
    if (err) {
        if (typeof(err) == "string")
            return err;
        if (err instanceof Error)
            return err.message;
        if (err instanceof nsIException)
            return err.toString(); // xxx todo: or just message?
        if (isNaN(err))
            return ("[" + err + "] unknown error.");
    }
    // numeric codes:
    switch (err) {
    case undefined:
    case null:
    case NS_OK:
        return "NS_OK";
    case Components.results.NS_ERROR_INVALID_ARG:
        return "NS_ERROR_INVALID_ARG";
    case Components.results.NS_ERROR_NO_INTERFACE:
        return "NS_ERROR_NO_INTERFACE";
    case Components.results.NS_ERROR_NOT_IMPLEMENTED:
        return "NS_ERROR_NOT_IMPLEMENTED";
    case Components.results.NS_ERROR_NOT_AVAILABLE:
        return "NS_ERROR_NOT_AVAILABLE";
    case Components.results.NS_ERROR_FAILURE:
        return "NS_ERROR_FAILURE";
    case Components.results.NS_ERROR_BASE:
        return "NS_ERROR_BASE";
    case Components.results.NS_ERROR_NOT_INITIALIZED:
        return "NS_ERROR_NOT_INITIALIZED";
    case Components.results.NS_ERROR_ALREADY_INITIALIZED:
        return "NS_ERROR_ALREADY_INITIALIZED";
    case Components.results.NS_ERROR_NULL_POINTER:
        return "NS_ERROR_NULL_POINTER";
    case Components.results.NS_ERROR_ABORT:
        return "NS_ERROR_ABORT";
    case Components.results.NS_ERROR_UNEXPECTED:
        return "NS_ERROR_UNEXPECTED";
    case Components.results.NS_ERROR_OUT_OF_MEMORY:
        return "NS_ERROR_OUT_OF_MEMORY";
    case Components.results.NS_ERROR_ILLEGAL_VALUE:
        return "NS_ERROR_ILLEGAL_VALUE";
    default:
        // probe for WCAP error:
        try {
            return wcapErrorToString(err);
        }
        catch (exc) { // probe for netwerk error:
            try {
                return netErrorToString(err);
            }
            catch (exc) {
                return ("[0x" + err.toString(0x10) + "] unknown error.");
            }
        }
    }
}

