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
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is
 * David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
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

#ifndef nsReadCLine_h__
#define nsReadCLine_h__

//This is shameless and obvious theft from nsReadLine.h 

#include "nsReadLine.h"
#include "nsString.h"

static nsresult
NS_ReadLine (nsIInputStream* aStream, nsLineBuffer * aBuffer,
             nsACString & aLine, PRBool *more) {
  nsresult rv = NS_OK;
  PRUint32 bytesRead;
  nsCAutoString temp;
  *more = PR_TRUE;
  PRBool eolStarted = PR_FALSE;
  char eolchar='\0';
  aLine.Truncate();
  while (1) { // will be returning out of this loop on eol or eof
    if (aBuffer->empty) { // buffer is empty.  Read into it.
      rv = aStream->Read(aBuffer->buf, kLineBufferSize, &bytesRead);
      if (NS_FAILED(rv)) // read failed
        return rv;
      if (bytesRead == 0) { // end of file
        *more = PR_FALSE;
        return NS_OK;
      }
      aBuffer->end = aBuffer->buf + bytesRead;
      aBuffer->empty = PR_FALSE;
      *(aBuffer->end) = '\0'; // null-terminate this thing
    }
    // walk the buffer looking for an end-of-line
    while (aBuffer->current < aBuffer->end) {
      if (eolStarted) {
          if ((eolchar == '\n' && *(aBuffer->current) == '\r') ||
              (eolchar == '\r' && *(aBuffer->current) == '\n')) { // line end
            (aBuffer->current)++;
            aBuffer->start = aBuffer->current;
          }
          eolStarted = PR_FALSE;
          return NS_OK;
      } else if (*(aBuffer->current) == '\n' ||
                 *(aBuffer->current) == '\r') { // line end
        eolStarted = PR_TRUE;
        eolchar = *(aBuffer->current);
        *(aBuffer->current) = '\0';
        temp.Assign(aBuffer->start);
        aLine.Append(temp);
        (aBuffer->current)++;
        aBuffer->start = aBuffer->current;
      } else {
        eolStarted = PR_FALSE;
        (aBuffer->current)++;
      }
    }

    // append whatever we currently have to the string
    temp.Assign(aBuffer->start);
    aLine.Append(temp);

    // we've run out of buffer.  Begin anew
    aBuffer->current = aBuffer->start = aBuffer->buf;
    aBuffer->empty = PR_TRUE;
    
    if (eolStarted) {  // have to read another char and possibly skip over it
      rv = aStream->Read(aBuffer->buf, 1, &bytesRead);
      if (NS_FAILED(rv)) // read failed
        return rv;
      if (bytesRead == 0) { // end of file
        *more = PR_FALSE;
        return NS_OK;
      }
      if ((eolchar == '\n' && *(aBuffer->buf) == '\r') ||
          (eolchar == '\r' && *(aBuffer->buf) == '\n')) {
        // Just return and all is good -- we've skipped the extra newline char
        return NS_OK;
      } else {
        // we have a byte that we should look at later
        aBuffer->empty = PR_FALSE;
        aBuffer->end = aBuffer->buf + 1;
        *(aBuffer->end) = '\0';
      }
    }
  }
}

#endif // nsReadCLine_h__
