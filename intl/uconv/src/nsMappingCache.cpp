/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMappingCache.h"

typedef struct {
  PRInt16 mask;
  PRInt16 reserved;
  PRUint32 data[1] ;
} nsMappingCacheBase;

typedef struct {
  PRInt16 mask;
  PRInt16 reserved;
  PRUint32 data[64] ;
} nsMappingCache64;

typedef struct {
  PRInt16 mask;
  PRInt16 reserved;
  PRUint32 data[128] ;
} nsMappingCache128;

typedef struct {
  PRInt16 mask;
  PRInt16 reserved;
  PRUint32 data[256] ;
} nsMappingCache256;

typedef struct {
  PRInt16 mask;
  PRInt16 reserved;
  PRUint32 data[512] ;
} nsMappingCache512;

nsresult nsMappingCache::CreateCache(nsMappingCacheType aType, nsIMappingCache* aResult)
{
  // to be implemented
  return NS_OK;
}

nsresult nsMappingCache::DestroyCache(nsIMappingCache aCache)
{
  // to be implemented
  return NS_OK;
}
