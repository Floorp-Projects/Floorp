/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998, 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef _nsImapMoveCoalescer_H
#define _nsImapMoveCoalescer_H

#include "msgCore.h"
class nsImapMailFolder;

#include "nsISupportsArray.h"
#include "nsCOMPtr.h"

// imap move coalescer class - in order to keep nsImapMailFolder from growing like Topsy
// Logically, we want to keep track of an nsMsgKeyArray per nsIMsgFolder, and then
// be able to retrieve them one by one and play back the moves.
// This utility class will be used by both the filter code and the offline playback code,
// to avoid multiple moves to the same folder.

#include "nsISupportsArray.h"

class nsImapMoveCoalescer 
{
public:
  nsImapMoveCoalescer(nsIMsgFolder *sourceFolder, nsIMsgWindow *msgWindow);
  virtual ~nsImapMoveCoalescer();

  nsresult AddMove(nsIMsgFolder *folder, nsMsgKey key);
  nsresult PlaybackMoves(nsIEventQueue *eventQueue);
protected:
  // m_sourceKeySets and m_destFolders are parallel arrays.
  nsVoidArray					m_sourceKeyArrays;
  nsCOMPtr <nsISupportsArray> m_destFolders;
  nsCOMPtr <nsIMsgWindow> m_msgWindow;
  nsCOMPtr <nsIMsgFolder> m_sourceFolder;
};

#endif // _nsImapMoveCoalescer_H

