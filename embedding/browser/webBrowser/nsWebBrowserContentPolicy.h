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
 * The Original Code is embedding content-control code.
 *
 * The Initial Developer of the Original Code is Mike Shaver.
 * Portions created by Mike Shaver are Copyright (C) 2001 Mike Shaver.
 * All Rights Reserved.
 *
 */

#include "nsIContentPolicy.h"

/* f66bc334-1dd1-11b2-bab2-90e04fe15c19 */
#define NS_WEBBROWSERCONTENTPOLICY_CID \
{ 0xf66bc334, 0x1dd1, 0x11b2, { 0xba, 0xb2, 0x90, 0xe0, 0x4f, 0xe1, 0x5c, 0x19 } }

#define NS_WEBBROWSERCONTENTPOLICY_CONTRACTID "@mozilla.org/embedding/browser/content-policy;1"

class nsWebBrowserContentPolicy : public nsIContentPolicy
{
public:
    nsWebBrowserContentPolicy();
    virtual ~nsWebBrowserContentPolicy();
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPOLICY
};

