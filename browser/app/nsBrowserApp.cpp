/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsXULAppAPI.h"
#ifdef XP_WIN
#include <windows.h>
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include "plstr.h"
#include "prprf.h"
#include "prenv.h"

#include "nsCOMPtr.h"
#include "nsILocalFile.h"
#include "nsStringGlue.h"

#ifdef XP_WIN
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#endif

static void Output(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);

#if defined(XP_WIN) && !MOZ_WINCONSOLE
  char msg[2048];

  _vsnprintf(msg, sizeof(msg), fmt, ap);

  MessageBox(NULL, msg, "XULRunner", MB_OK | MB_ICONERROR);
#else
  vfprintf(stderr, fmt, ap);
#endif

  va_end(ap);
}

/**
 * Return true if |arg| matches the given argument name.
 */
static PRBool IsArg(const char* arg, const char* s)
{
  if (*arg == '-')
  {
    if (*++arg == '-')
      ++arg;
    return !PL_strcasecmp(arg, s);
  }

#if defined(XP_WIN) || defined(XP_OS2)
  if (*arg == '/')
    return !PL_strcasecmp(++arg, s);
#endif

  return PR_FALSE;
}

/**
 * A helper class which calls NS_LogInit/NS_LogTerm in its scope.
 */
class ScopedLogging
{
public:
  ScopedLogging() { NS_LogInit(); }
  ~ScopedLogging() { NS_LogTerm(); }
};

int main(int argc, char* argv[])
{
  ScopedLogging log;

  nsCOMPtr<nsILocalFile> appini;
  nsresult rv = XRE_GetBinaryPath(argv[0], getter_AddRefs(appini));
  if (NS_FAILED(rv)) {
    Output("Couldn't calculate the application directory.");
    return 255;
  }
  appini->SetNativeLeafName(NS_LITERAL_CSTRING("application.ini"));

  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  char *appEnv = nsnull;
  const char *appDataFile = PR_GetEnv("XUL_APP_FILE");
  if (appDataFile && *appDataFile) {
    rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("Invalid path found: '%s'", appDataFile);
      return 255;
    }
  }
  else if (argc > 1 && IsArg(argv[1], "app")) {
    if (argc == 2) {
      Output("Incorrect number of arguments passed to -app");
      return 255;
    }

    rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
    if (NS_FAILED(rv)) {
      Output("application.ini path not recognized: '%s'", argv[2]);
      return 255;
    }

    appEnv = PR_smprintf("XUL_APP_FILE=%s", argv[2]);
    PR_SetEnv(appEnv);
    argv[2] = argv[0];
    argv += 2;
    argc -= 2;
  }

  nsXREAppData *appData;
  rv = XRE_CreateAppData(appini, &appData);
  if (NS_FAILED(rv)) {
    Output("Couldn't read application.ini");
    return 255;
  }

  int result = XRE_main(argc, argv, appData);
  XRE_FreeAppData(appData);
  if (appEnv)
    PR_smprintf_free(appEnv);
  return result;
}
