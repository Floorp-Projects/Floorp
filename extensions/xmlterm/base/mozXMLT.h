/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

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
