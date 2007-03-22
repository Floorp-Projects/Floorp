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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsGfxDefs.h"

#ifdef DEBUG
  #include <string.h>
  #include <ctype.h>

//DJ  #include "nsLog.h"
//DJ  NS_IMPL_LOG_ENABLED (GFXLog)
//DJ  #define DPRINTF NS_LOG_PRINTF(GFXLog)
  #include <stdio.h>
  #define DPRINTF printf
#else
  #include <stdio.h>
  #define DPRINTF printf
#endif

#include "nsDeviceContextSpecOS2.h"

#include <stdlib.h>

BOOL GetTextExtentPoint32(HPS aPS, const char* aString, int aLength, PSIZEL aSizeL)
{
  BOOL rc = TRUE;
  POINTL ptls[5];

  aSizeL->cx = 0;

  while(aLength > 0 && rc == TRUE) {
    ULONG thislen = min(aLength, 512);
    rc = GFX (::GpiQueryTextBox(aPS, thislen, (PCH)aString, 5, ptls), FALSE);
    aSizeL->cx += ptls[TXTBOX_CONCAT].x;
    aLength -= thislen;
    aString += thislen;
  }

  aSizeL->cy = ptls[TXTBOX_TOPLEFT].y - ptls[TXTBOX_BOTTOMLEFT].y;
  return rc;
}

BOOL ExtTextOut(HPS aPS, int X, int Y, UINT fuOptions, const RECTL* lprc,
                const char* aString, unsigned int aLength, const int* pSpacing)
{
  long rc = GPI_OK;
  POINTL ptl = {X, Y};

  GFX (::GpiMove(aPS, &ptl), FALSE);

  // GpiCharString has a max length of 512 chars at a time...
  while (aLength > 0 && rc == GPI_OK) {
    ULONG ulChunkLen = min(aLength, 512);
    if (pSpacing) {
      rc = GFX (::GpiCharStringPos(aPS, nsnull, CHS_VECTOR, ulChunkLen,
                                   (PCH)aString, (PLONG)pSpacing), GPI_ERROR);
      pSpacing += ulChunkLen;
    } else {
      rc = GFX (::GpiCharString(aPS, ulChunkLen, (PCH)aString), GPI_ERROR);
    }
    aLength -= ulChunkLen;
    aString += ulChunkLen;
  }

  if (rc == GPI_OK)
    return TRUE;
  else
    return FALSE;
}

static BOOL bIsDBCS;
static BOOL bIsDBCSSet = FALSE;

BOOL IsDBCS()
{
  if (!bIsDBCSSet) {
    // the following lines of code determine whether the system is a DBCS country
    APIRET rc;
    COUNTRYCODE ctrycodeInfo = {0};
    CHAR        achDBCSInfo[12] = {0};                  // DBCS environmental vector
    ctrycodeInfo.country  = 0;                          // current country
    ctrycodeInfo.codepage = 0;                          // current codepage

    rc = DosQueryDBCSEnv(sizeof(achDBCSInfo), &ctrycodeInfo, achDBCSInfo);
    if (rc == NO_ERROR)
    {
        // NON-DBCS countries will have four bytes in the first four bytes of the
        // DBCS environmental vector
        if (achDBCSInfo[0] != 0 || achDBCSInfo[1] != 0 ||
            achDBCSInfo[2] != 0 || achDBCSInfo[3] != 0)
        {
           bIsDBCS = TRUE;
        }
        else
        {
           bIsDBCS = FALSE;
        }
    } else {
       bIsDBCS = FALSE;
    } /* endif */
    bIsDBCSSet = TRUE;
  } /* endif */
  return bIsDBCS;
}




// Module-level data ---------------------------------------------------------
#ifdef DEBUG
void DEBUG_LogErr(long ReturnCode, const char* ErrorExpression,
                  const char* FileName, const char* FunctionName, long LineNum)
{
   char TempBuf [300];

   strcpy (TempBuf, ErrorExpression);
   char* APIName = TempBuf;

   char* ch = strstr (APIName , "(");                 // Find start of function parameter list
   if (ch != NULL)                                    // Opening parenthesis found - it is a function
   {
      while (isspace (*--ch)) {}                      // Remove whitespaces before opening parenthesis
      *++ch = '\0';

      if (APIName [0] == ':' && APIName [1] == ':')   // Remove global scope operator
         APIName += 2;

      while (isspace (*APIName))                      // Remove spaces before function name
         APIName++;
   }


   USHORT ErrorCode = ERRORIDERROR (::WinGetLastError(0));

   printf("GFX_Err: %s = 0x%X, 0x%X (%s - %s,  line %ld)\n", APIName, ReturnCode,
          ErrorCode, FileName, FunctionName, LineNum);
}
#endif

void PMERROR( const char *api)
{
   ERRORID eid = ::WinGetLastError(0);
   USHORT usError = ERRORIDERROR(eid);
   DPRINTF ( "%s failed, error = 0x%X\n", api, usError);
}

