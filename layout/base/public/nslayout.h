/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nslayout_h___
#define nslayout_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nsTraceRefcnt.h"

// Note: For now, NS_LAYOUT and NS_HTML are joined at the hip
#if defined(_IMPL_NS_LAYOUT) || defined(_IMPL_NS_HTML)
#define NS_LAYOUT NS_EXPORT
#define NS_HTML   NS_EXPORT
#else
#define NS_LAYOUT NS_IMPORT
#define NS_HTML   NS_IMPORT
#endif

#define INCLUDE_XUL 1

#endif /* nslayout_h___ */
