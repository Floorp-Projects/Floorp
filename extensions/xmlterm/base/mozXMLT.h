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
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// mozXMLT.h: XMLTerm common header

#ifndef _MOZXMLT_H

#define _MOZXMLT_H 1

// Standard C header files
#ifndef _STRING_H
#include <string.h>
#endif

// public declarations
#include "lineterm.h"
#include "tracelog.h"

// private declarations

#define XMLT_TLOG_MODULE  2
#define XMLT_ERROR                            TLOG_ERROR
#define XMLT_WARNING                          TLOG_WARNING
#define XMLT_LOG(procname,level,args)         TLOG_PRINT(XMLT_TLOG_MODULE,procname,level,args)

// Tracing versions of NS_IMPL_ADDREF and NS_IMPL_RELEASE

#define XMLT_IMPL_ADDREF(_class) \
NS_IMETHODIMP_(nsrefcnt) _class::AddRef(void)  \
{                                              \
  NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt"); \
  ++mRefCnt;                                   \
  XMLT_WARNING(#_class ":AddRef, mRefCnt=%d\n", mRefCnt); \
  return mRefCnt;                              \
}

#define XMLT_IMPL_RELEASE(_class) \
NS_IMETHODIMP_(nsrefcnt) _class::Release(void)  \
{                                               \
  NS_PRECONDITION(0 != mRefCnt, "dup release"); \
  --mRefCnt;                                    \
  XMLT_WARNING(#_class ":Release, mRefCnt=%d\n", mRefCnt); \
  if (mRefCnt == 0) {                           \
    NS_DELETEXPCOM(this);                       \
    return 0;                                   \
  }                                             \
  return mRefCnt;                               \
}

#endif  /* _MOZXMLT_H */
