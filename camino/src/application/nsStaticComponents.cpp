/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Seawood <cls@seawood.org>
 *   Chris Waterson <waterson@netscape.com>
 *   Benjamin Smedberg <benjamin@smedbergs.us>
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

#define XPCOM_TRANSLATE_NSGM_ENTRY_POINT 1

#include "nsIGenericFactory.h"
#include "nsXPCOM.h"
#include "nsStaticComponents.h"
#include "nsMemory.h"

/**
 * Construct a unique NSGetModule entry point for a generic module.
 */
#define NSGETMODULE(_name) _name##_NSGetModule

#define MODULES \
    MODULE(xpcomObsoleteModule) \
    MODULE(nsI18nModule) \
    MODULE(nsChardetModule) \
    MODULE(nsUConvModule) \
    MODULE(nsUCvMathModule) \
    MODULE(nsJarModule) \
    MODULE(xpconnect) \
    MODULE(necko) \
    MODULE(nsPrefModule) \
    MODULE(nsCJVMManagerModule) \
    MODULE(nsSecurityManagerModule) \
    MODULE(nsChromeModule) \
    MODULE(nsRDFModule) \
    MODULE(nsParserModule) \
    MODULE(nsGfxModule) \
    MODULE(nsImageLib2Module) \
    MODULE(nsPluginModule) \
    MODULE(nsWidgetMacModule) \
    MODULE(nsLayoutModule) \
    MODULE(nsMorkModule) \
    MODULE(docshell_provider) \
    MODULE(embedcomponents) \
    MODULE(Browser_Embedding_Module) \
    MODULE(nsTransactionManagerModule) \
    MODULE(application) \
    MODULE(nsCookieModule) \
    MODULE(nsXMLExtrasModule) \
    MODULE(nsUniversalCharDetModule) \
    MODULE(nsTypeAheadFind) \
    MODULE(nsPermissionsModule) \
    MODULE(nsComposerModule) \
    MODULE(xpAutoComplete) \
    MODULE(mozSpellCheckerModule) \
    MODULE(nsAuthModule) \
    MODULE(BOOT) \
    MODULE(NSS) \
    /* end of list */

/**
 * Declare the NSGetModule() routine for each module
 */
#define MODULE(_name) \
NSGETMODULE_ENTRY_POINT(_name) (nsIComponentManager*, nsIFile*, nsIModule**);

MODULES

#undef MODULE

#define MODULE(_name) { #_name, NSGETMODULE(_name) },

/**
 * The nsStaticModuleInfo
 */
static nsStaticModuleInfo const gStaticModuleInfo[] = {
    MODULES
};

nsStaticModuleInfo const *const kPStaticModules = gStaticModuleInfo;
PRUint32 const kStaticModuleCount = NS_ARRAY_LENGTH(gStaticModuleInfo);
