/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

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
  POINTL ptls[5];

  aSizeL->cx = 0;

  while(aLength)
  {
    ULONG thislen = min(aLength, 512);
    GFX (::GpiQueryTextBox (aPS, thislen, (PCH)aString, 5, ptls), FALSE);
    aSizeL->cx += ptls[TXTBOX_CONCAT].x;
    aLength -= thislen;
    aString += thislen;
  }
  aSizeL->cy = ptls[TXTBOX_TOPLEFT].y - ptls[TXTBOX_BOTTOMLEFT].y;
  return TRUE;
}

BOOL ExtTextOut(HPS aPS, int X, int Y, UINT fuOptions, const RECTL* lprc,
                const char* aString, unsigned int aLength, const int* pSpacing)
{
  POINTL ptl = {X, Y};

  GFX (::GpiMove (aPS, &ptl), FALSE);

  // GpiCharString has a max length of 512 chars at a time...
  while( aLength)
  {
    ULONG ulChunkLen = min(aLength, 512);
    if (pSpacing)
    {
      GFX (::GpiCharStringPos (aPS, nsnull, CHS_VECTOR, ulChunkLen,
                               (PCH)aString, (PLONG)pSpacing), GPI_ERROR);
        pSpacing += ulChunkLen;
    }
    else
    {
      GFX (::GpiCharString (aPS, ulChunkLen, (PCH)aString), GPI_ERROR);
    }
    aLength -= ulChunkLen;
    aString += ulChunkLen;
  }
  return TRUE;
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
void GFX_LogErr (unsigned ReturnCode, const char* ErrorExpression, const char* FileName, const char* FunctionName, long LineNum)
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


   USHORT ErrorCode = ERRORIDERROR (::WinGetLastError (0));

   if (FunctionName)      // Compiler knows function name from where we were called
      DPRINTF ("GFX_Err: %s = 0x%X, 0x%X (%s - %s,  line %d)\n", APIName, ReturnCode, ErrorCode, FileName, FunctionName, LineNum);
   else
      DPRINTF ("GFX_Err: %s = 0x%X, 0x%X (%s,  line %d)\n", APIName, ReturnCode, ErrorCode, FileName, LineNum);
}
#endif

void PMERROR( const char *api)
{
   ERRORID eid = ::WinGetLastError(0);
   USHORT usError = ERRORIDERROR(eid);
   DPRINTF ( "%s failed, error = 0x%X\n", api, usError);
}

