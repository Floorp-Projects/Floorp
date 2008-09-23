/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla CE Shunt Library.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Wolfe, 21-July-2008
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

#include "mozce_internal.h"

#include <stdarg.h>


#ifndef SHUNT_LOG_ENABLED

void mozce_DebugInit() { };
void mozce_DebugDeinit() { };
void mozce_DebugWriteToLog(char *str) { };

#else

#define LOGFILE     "\\Storage Card\\shuntlog.txt"

FILE *gpDebugFile = NULL;

void mozce_DebugInit()
{
    if ( NULL == gpDebugFile )
        gpDebugFile = fopen(LOGFILE, "a+");
}

void mozce_DebugDeinit()
{
    if ( gpDebugFile ) {
        fclose( gpDebugFile );
        gpDebugFile = NULL;
    }
}

void mozce_DebugWriteToLog(char *str)
{
    if ( NULL == gpDebugFile )
        mozce_DebugInit();

    if ( gpDebugFile ) {
        fprintf(gpDebugFile, "%s", str);
        fflush(gpDebugFile);
    }
}

#endif
