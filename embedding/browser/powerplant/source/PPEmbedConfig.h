/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s):
 *   Conrad Carlen <ccarlen@netscape.com>
 */

// Configuration flags used by all targets

#ifndef __PPEmbedConfig_h
#define __PPEmbedConfig_h

/*
    USE_PROFILES
    
    If defined, the application will use nsIProfileManager to allow
    distinct user profiles. It also demonstrates dynamic profile switching in this case.
    
    If undefined, the application will construct and register an
    nsIDirectoryServiceProvider which provides profile-relative file locations to one
    fixed directory.
*/
    
#undef USE_PROFILES

/*
    SHARED_PROFILE
    
    If defined, the application will share profile data (that which is shareable) with
    other applications in its "suite." The data which is not shared by applications
    in the suite is stored in a subdir of the profile which is named for each particular
    member of the suite. This can be used with or without USE_PROFILES.
*/

#define SHARED_PROFILE

/*
    NATIVE_PROMPTS
    
    If defined, the application will override Gecko's prompt service
    component with the implementation in PromptService.cpp. This implementation uses
    PowerPlant dialogs.
    
    If undefined, the application will use Gecko's default prompt service. This implementation
    creates chrome dialogs through the nsIWindowCreator interface. Undefining this
    is useful for testing the implementation of nsIWindowCreator, nsIWebBrowserChrome, etc.
*/

#define NATIVE_PROMPTS

#endif
