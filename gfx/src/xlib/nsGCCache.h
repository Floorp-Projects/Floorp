/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Shaver <shaver@zeroknowledge.com>
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


#ifndef nsGCCacheXlib_h___
#define nsGCCacheXlib_h___

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "nscore.h"
#include "nsDebug.h"
#include "prclist.h"
#include <stdlib.h>
#include <string.h>

#define countof(x) ((int)(sizeof(x) / sizeof (*x)))
#define GC_CACHE_SIZE 64

#ifdef DEBUG
#define DEBUG_METER(x) x
#else
#define DEBUG_METER(x)
#endif

class nsGCCacheXlib;

class xGC {
  friend class nsGCCacheXlib;

public:
  xGC(Display *display, Drawable d, unsigned long valuemask, XGCValues *values)
  {
    mRefCnt = 0;
    mDisplay = display;
    mGC = ::XCreateGC(display, d, valuemask, values);
  }

  virtual ~xGC() {
    ::XFreeGC(mDisplay, mGC);
  }

  PRInt32 AddRef(void) {
#ifdef DEBUG  
    if(mRefCnt>400) abort();
#endif /* DEBUG */
    NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
    ++mRefCnt;
    return mRefCnt;
  }
  PRInt32 Release(void) {
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    --mRefCnt;
    if (mRefCnt == 0) {
      mRefCnt = 1; /* stabilize */
      delete this;
      return 0;
    }
    return mRefCnt;
  }

  //operator GC() { return mGC; }
  operator const GC() { return (const GC)mGC; }

private:
  PRInt32 mRefCnt;
  Display *mDisplay;
  GC mGC;
};


struct GCCacheEntryXlib
{
  PRCList clist;
  unsigned long flags;
  XGCValues gcv;
  Region clipRegion;
  xGC *gc;
};

class nsGCCacheXlib
{
 public:
  nsGCCacheXlib();
  virtual ~nsGCCacheXlib();

  void Flush(unsigned long flags);

  xGC *GetGC(Display *display, Window window, unsigned long flags, XGCValues *gcv, Region clipRegion);
  
private:
  void ReuseGC(GCCacheEntryXlib *entry, unsigned long flags, XGCValues *gcv);
  PRCList GCCache;
  PRCList GCFreeList;
  void free_cache_entry(PRCList *clist);
  void move_cache_entry(PRCList *clist);
  static void XCopyRegion(Region src, Region dr_return);
  static Region copyRegion;
  void ReportStats();

  DEBUG_METER(
              struct {
                int hits[GC_CACHE_SIZE];
                int misses;
                int reclaim;
              } GCCacheStats;
              )

};

#endif
