/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
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

#ifndef _nsMsgFilterList_H_
#define _nsMsgFilterList_H_

#include "nscore.h"
#include "nsIMsgFolder.h"
#include "nsIMsgFilterList.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsIFileSpec.h"
#include "nsIOutputStream.h"

const PRInt16 kFileVersion = 8;
const PRInt16 k60Beta1Version = 7;
const PRInt16 k45Version = 6;


////////////////////////////////////////////////////////////////////////////////////////
// The Msg Filter List is an interface designed to make accessing filter lists
// easier. Clients typically open a filter list and either enumerate the filters,
// or add new filters, or change the order around...
//
////////////////////////////////////////////////////////////////////////////////////////

class nsIMsgFilter;
class nsMsgFilter;

class nsMsgFilterList : public nsIMsgFilterList
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGFILTERLIST

  nsMsgFilterList();
  virtual ~nsMsgFilterList();

  nsresult Close();
  nsresult LoadTextFilters(nsIOFileStream *aStream);

protected:
  // type-safe accessor when you really have to have an nsMsgFilter
  nsresult GetMsgFilterAt(PRUint32 filterIndex, nsMsgFilter **filter);
#ifdef DEBUG
  void Dump();
#endif
protected:
  nsresult ComputeArbitraryHeaders();
  nsresult SaveTextFilters(nsIOFileStream *aStream);
  // file streaming methods
  char ReadChar(nsIOFileStream *aStream);
  char SkipWhitespace(nsIOFileStream *aStream);
  PRBool StrToBool(nsCString &str);
  char LoadAttrib(nsMsgFilterFileAttribValue &attrib, nsIOFileStream *aStream);
  const char *GetStringForAttrib(nsMsgFilterFileAttribValue attrib);
  nsresult LoadValue(nsCString &value, nsIOFileStream *aStream);
  PRInt16 m_fileVersion;
  PRPackedBool m_loggingEnabled;
  PRPackedBool m_startWritingToBuffer; //tells us when to start writing one whole filter to m_unparsedBuffer
  nsCOMPtr <nsIMsgFolder> m_folder;
  nsMsgFilter *m_curFilter; // filter we're filing in or out(?)
  const char *m_filterFileName;
  nsCOMPtr<nsISupportsArray> m_filters;
  nsCString m_arbitraryHeaders;
  nsCOMPtr<nsIFileSpec> m_defaultFile;
  nsCString m_unparsedFilterBuffer; //holds one entire filter unparsed 

private:
  nsresult TruncateLog();
  nsresult GetLogFileSpec(nsIFileSpec **aFileSpec);
  nsCOMPtr<nsIOutputStream> m_logStream;
};

#endif



