/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s): 
 */
#ifndef GUIDS_H
#define GUIDS_H

#define NS_EXTERN_IID(_name) \
    extern const nsIID _name;

// Class IDs
NS_EXTERN_IID(kEventQueueServiceCID);
NS_EXTERN_IID(kHTMLEditorCID);
NS_EXTERN_IID(kCookieServiceCID);
NS_EXTERN_IID(kPrefCID);
NS_EXTERN_IID(kWindowCID);


#endif