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

/* JS -- Update these from calItemModule.js */
#define CAL_EVENT_CID \
    { 0x974339d5, 0xab86, 0x4491, { 0xaa, 0xaf, 0x2b, 0x2c, 0xa1, 0x77, 0xc1, 0x2b } }
#define CAL_EVENT_CONTRACTID \
    "@mozilla.org/calendar/event;1"

#define CAL_TODO_CID \
    { 0x7af51168, 0x6abe, 0x4a31, { 0x98, 0x4d, 0x6f, 0x8a, 0x39, 0x89, 0x21, 0x2d } }
#define CAL_TODO_CONTRACTID \
    "@mozilla.org/calendar/todo;1"

#endif /* CALBASECID_H_ */
