/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Kyle Yuan <kyle.yuan@sun.com>
 *
 * Contributor(s):
 *
 */

#ifndef AppComponents_h__
#define AppComponents_h__

#ifndef nsIGenericFactory_h___
#include "nsIGenericFactory.h"
#endif

// This method should return a nsModuleComponentInfo array of
// application-provided XPCOM components to register.
extern const nsModuleComponentInfo* GetAppModuleComponentInfo(int* outNumComponents);

#endif
