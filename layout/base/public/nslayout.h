/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nslayout_h___
#define nslayout_h___

#include "nscore.h"

// Note: For now, NS_LAYOUT and NS_HTML are joined at the hip
#if defined(_IMPL_NS_LAYOUT) || defined(_IMPL_NS_HTML)
#define NS_LAYOUT NS_EXPORT
#define NS_HTML   NS_EXPORT
#else
#define NS_LAYOUT NS_IMPORT
#define NS_HTML   NS_IMPORT
#endif

#endif /* nslayout_h___ */
