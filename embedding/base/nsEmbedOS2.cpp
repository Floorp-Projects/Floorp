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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s):
 */

#include "nsEmbedAPI.h"

#ifndef OS2
#error This file is for OS/2!
#endif

#ifdef MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING

nsresult NS_HandleEmbeddingEvent(nsEmbedNativeEvent &aEvent, PRBool &aWasHandled)
{
    aWasHandled = PR_FALSE;
    return NS_OK;
}

#endif /* MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING */
