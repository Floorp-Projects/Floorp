/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
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

#ifndef nsRecycled_h__
#define nsRecycled_h__

#include "nsAutoPtr.h"
#include "prtypes.h"

/*
 * A simple one-object recycler.
 * Derive from this class if usually only one heap-allocated instance of your class
 * exists at a time. This class accelerates heap allocations by keeping around
 * a pre-allocated buffer the size of a single instance of your class.
 *
 * The OwnerType template parameter is given to differentiate inheriting classes. For instance,
 * nsFoo should inherit from nsRecycledSingle<nsFoo>. If a hierarchy inherits from nsFoo,
 * the same preallocated buffer would be used for instances of all derived classes, so you
 * should ensure that usually only one instance of any of those classes exists at a time.
 *
 * Example:
 *  class nsFoo : public nsIBar, nsRecycledSingle<nsFoo>
 *  {
 *     ...
 *  };
 *
 */
template<class OwnerType>
class nsRecycledSingle
{
public:
  /* Initializes the memory to 0. */
  void* operator new(size_t sz) CPP_THROW_NEW
  {
      void* result = nsnull;
  
      if (!gPoolInUse)
      {
         if (gPoolSize >= sz)
         {
            result = gPool;
         }
         else
         {
            // Original pool, if any, is deleted by this plain assignment.
            result = ::operator new(sz);
            gPool = (char*)result;
            gPoolSize = sz;
         }
         gPoolInUse = PR_TRUE;
      }
      else
      {
         if (sz > gPoolSize)
         {
            // This would be a good opportunity to throw away our
            // current pool and replace it with this, larger one.

            // Original pool will die as a regular allocated buffer
            // when its time is due.
            gPool.forget();
            result = ::operator new(sz);
            gPool = (char*)result;
            gPoolSize = sz;
         }
         else
         {
            result = ::operator new(sz);
         }
      }
      
     if (result) {
        memset(result, 0, sz);
     }
     
     return result;
  }

  void operator delete(void* aPtr)
  {
      if (gPool == aPtr)
      {
         gPoolInUse = PR_FALSE;
         return;
      }
      else
      {
         ::operator delete(aPtr);
      }
  }
  
protected:
  static nsAutoPtr<char> gPool;
  static size_t gPoolSize;
  static PRBool gPoolInUse;
};

template<class OwnerType> 
nsAutoPtr<char> nsRecycledSingle<OwnerType>::gPool;
template<class OwnerType> 
size_t nsRecycledSingle<OwnerType>::gPoolSize = 0;
template<class OwnerType> 
PRBool nsRecycledSingle<OwnerType>::gPoolInUse = PR_FALSE;

#endif // nsRecycled_h__
