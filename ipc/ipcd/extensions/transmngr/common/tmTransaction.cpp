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
 * The Original Code is Mozilla Transaction Manager.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   John Gaunt <jgaunt@netscape.com>
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

#include <stdlib.h>
#include "tmTransaction.h"

///////////////////////////////////////////////////////////////////////////////
// Constructor(s) & Destructor

tmTransaction::~tmTransaction() {
  if (mHeader)
    free(mHeader);
}

// call only once per lifetime of object. does not reclaim the
//    raw message, only sets it.
nsresult
tmTransaction::Init(PRUint32 aOwnerID,
                    PRInt32  aQueueID, 
                    PRUint32 aAction, 
                    PRInt32  aStatus, 
                    const PRUint8 *aMessage, 
                    PRUint32 aLength) {
  nsresult rv = NS_OK;
  tmHeader *header = nsnull;

  // indicates the message is the entire raw message
  if (aQueueID == TM_INVALID_ID) {
    header = (tmHeader*) malloc(aLength);
    if (header) {
      mRawMessageLength = aLength;
      memcpy(header, aMessage, aLength);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {   // need to create the tmHeader and concat the message
    header = (tmHeader*) malloc (sizeof(tmHeader) + aLength);
    if (header) {
      mRawMessageLength = sizeof(tmHeader) + aLength;
      header->action = aAction;
      header->queueID = aQueueID;
      header->status = aStatus;
      header->reserved = 0x00000000;
      if (aLength > 0)     // add the message if it exists
        memcpy((header + 1), aMessage, aLength);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv)) {
    mOwnerID = aOwnerID;
    mHeader = header;
  }

  return rv;
}
