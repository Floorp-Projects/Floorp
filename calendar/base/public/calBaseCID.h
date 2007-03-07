/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
 *   Mike Shaver <shaver@off.net>
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

#ifndef CALBASECID_H_
#define CALBASECID_H_

/* C++ */
#define CAL_DATETIME_CID \
    { 0x85475b45, 0x110a, 0x443c, { 0xaf, 0x3f, 0xb6, 0x63, 0x98, 0xa5, 0xa7, 0xcd } }
#define CAL_DATETIME_CONTRACTID \
    "@mozilla.org/calendar/datetime;1"

#define CAL_DURATION_CID \
    { 0x63513139, 0x51cb, 0x4f5b, { 0x9a, 0x52, 0x49, 0xac, 0xcc, 0x5c, 0xae, 0x17 } }
#define CAL_DURATION_CONTRACTID \
    "@mozilla.org/calendar/duration;1"

#define CAL_PERIOD_CID \
    { 0x12fdd72b, 0xc5b6, 0x4720, { 0x81, 0x66, 0x2d, 0xec, 0xa1, 0x33, 0x82, 0xf5 } }
#define CAL_PERIOD_CONTRACTID \
    "@mozilla.org/calendar/period;1"

#define CAL_ICSSERVICE_CID \
    { 0xae4ca6c3, 0x981b, 0x4f66, { 0xa0, 0xce, 0x2f, 0x2c, 0x21, 0x8a, 0xd9, 0xe3 } }
#define CAL_ICSSERVICE_CONTRACTID \
    "@mozilla.org/calendar/ics-service;1"

#define CAL_RECURRENCERULE_CID \
    { 0xd9560bf9, 0x3065, 0x404a, { 0x90, 0x4c, 0xc8, 0x82, 0xfc, 0x9c, 0x9b, 0x74 } }
#define CAL_RECURRENCERULE_CONTRACTID \
    "@mozilla.org/calendar/recurrence-rule;1"

#define CAL_RECURRENCEDATESET_CID \
    { 0x46ea7a6b, 0x01bd, 0x4ea5, { 0xb0, 0x6c, 0x90, 0xd5, 0xec, 0xcb, 0xbe, 0x3a } }
#define CAL_RECURRENCEDATESET_CONTRACTID \
    "@mozilla.org/calendar/recurrence-date-set;1"

#define CAL_RECURRENCEDATE_CID \
    { 0x99706e71, 0x3df5, 0x475e, { 0x84, 0xf8, 0x0f, 0xc5, 0x21, 0x1f, 0x92, 0x09 } }
#define CAL_RECURRENCEDATE_CONTRACTID \
    "@mozilla.org/calendar/recurrence-date;1"

/* JS -- Update these from calItemModule.js */
#define CAL_EVENT_CID \
    { 0x974339d5, 0xab86, 0x4491, { 0xaa, 0xaf, 0x2b, 0x2c, 0xa1, 0x77, 0xc1, 0x2b } }
#define CAL_EVENT_CONTRACTID \
    "@mozilla.org/calendar/event;1"

#define CAL_TODO_CID \
    { 0x7af51168, 0x6abe, 0x4a31, { 0x98, 0x4d, 0x6f, 0x8a, 0x39, 0x89, 0x21, 0x2d } }
#define CAL_TODO_CONTRACTID \
    "@mozilla.org/calendar/todo;1"

#define CAL_ATTENDEE_CID \
    { 0x5c8dcaa3, 0x170c, 0x4a73, { 0x81, 0x42, 0xd5, 0x31, 0x15, 0x6f, 0x66, 0x4d } }
#define CAL_ATTENDEE_CONTRACTID \
    "@mozilla.org/calendar/attendee;1"

#define CAL_RECURRENCEINFO_CID \
    { 0x04027036, 0x5884, 0x4a30, { 0xb4, 0xaf, 0xf2, 0xca, 0xd7, 0x9f, 0x6e, 0xdf } }
#define CAL_RECURRENCEINFO_CONTRACTID \
    "@mozilla.org/calendar/recurrence-info;1"

#define NS_ERROR_CALENDAR_WRONG_COMPONENT_TYPE		NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CALENDAR, 1)
// Until extensible xpconnect error mapping works
// #define NS_ERROR_CALENDAR_IMMUTABLE                 NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CALENDAR, 2)
#define NS_ERROR_CALENDAR_IMMUTABLE NS_ERROR_FAILURE

#endif /* CALBASECID_H_ */
