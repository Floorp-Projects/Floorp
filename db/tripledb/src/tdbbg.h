/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is
 * Geocast Network Systems.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Weissman <terry@mozilla.org>
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

#ifndef _tdbbg_h_
#define _tdbbg_h_ 1

typedef struct _TDBBG TDBBG;


typedef void (*TDBBG_Function)(void* closure);


TDB_BEGIN_EXTERN_C

#define TDBBG_PRIORITY_HIGH     100
#define TDBBG_PRIORITY_NORMAL    50
#define TDBBG_PRIORITY_LOW        0


TDB_EXTERN(TDBBG*) TDBBG_Open();


TDB_EXTERN(TDBStatus) TDBBG_Close(TDBBG* tdbbg);


TDB_EXTERN(TDBStatus) TDBBG_AddFunction(TDBBG* tdbbg, const char* name,
                                        TDBInt32 secondsToWait,
                                        TDBInt32 priority,
                                        TDBBG_Function func, void* closure);

TDB_EXTERN(TDBStatus) TDBBG_RescheduleFunction(TDBBG* tdbbg, const char* name,
                                               TDBInt32 secondsToWait,
                                               TDBInt32 priority,
                                               TDBBG_Function func,
                                               void* closure);

TDB_EXTERN(TDBStatus) TDBBG_RemoveFunction(TDBBG* tdbbg, const char* name,
                                           TDBBG_Function func, void* closure);


/* TDBBG_WaitUntilIdle() waits until the background thread is idle doing
   nothing.  This is occasionally useful, when you want to make sure any
   immediate background tasks you may have had are not doing anything.  Note
   that this will *not* wait for any things that are scheduled off into the
   future; it only looks for immediately scheduled tasks. */

TDB_EXTERN(TDBStatus) TDBBG_WaitUntilIdle(TDBBG* tdbbg);


TDB_END_EXTERN_C


#endif /* _tdbbg_h_ */
