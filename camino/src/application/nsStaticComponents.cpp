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
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * 	Christopher Seawood <cls@seawood.org>
 *  Chris Waterson <waterson@netscape.com>
 */

#line 26 "nsStaticComponents.cpp.in"
#define XPCOM_TRANSLATE_NSGM_ENTRY_POINT 1

#include "nsIGenericFactory.h"
#include "nsStaticComponent.h"

/**
 * Construct a unique NSGetModule entry point for a generic module.
 */
#define NSGETMODULE(_name) _name##_NSGetmodule

/**
 * Declare an NSGetModule() routine for a generic module.
 */
#define DECL_NSGETMODULE(_name)                                \
extern nsModuleInfo NSMODULEINFO(_name);                       \
extern "C" NS_EXPORT nsresult                                  \
NSGETMODULE(_name) (nsIComponentManager* aCompMgr,             \
                    nsIFile*             aLocation,            \
                    nsIModule**          aResult)              \
{                                                              \
    return NS_NewGenericModule2(&NSMODULEINFO(_name), aResult);\
}

// NSGetModule entry points
DECL_NSGETMODULE(xpcomObsoleteModule)
DECL_NSGETMODULE(nsI18nModule)
DECL_NSGETMODULE(nsUConvModule)
DECL_NSGETMODULE(nsJarModule)
DECL_NSGETMODULE(xpconnect)
DECL_NSGETMODULE(necko_core_and_primary_protocols)
DECL_NSGETMODULE(necko_secondary_protocols)
DECL_NSGETMODULE(nsPrefModule)
DECL_NSGETMODULE(nsCJVMManagerModule)
DECL_NSGETMODULE(nsSecurityManagerModule)
DECL_NSGETMODULE(nsChromeModule)
DECL_NSGETMODULE(nsRDFModule)
DECL_NSGETMODULE(nsParserModule)
DECL_NSGETMODULE(nsGfxMacModule)
DECL_NSGETMODULE(nsImageLib2Module)
DECL_NSGETMODULE(nsPluginModule)
DECL_NSGETMODULE(nsWidgetMacModule)
DECL_NSGETMODULE(nsLayoutModule)
DECL_NSGETMODULE(nsMorkModule)
DECL_NSGETMODULE(docshell_provider)
DECL_NSGETMODULE(embedcomponents)
DECL_NSGETMODULE(Browser_Embedding_Module)
DECL_NSGETMODULE(nsEditorModule)
DECL_NSGETMODULE(nsTransactionManagerModule)
DECL_NSGETMODULE(application)
DECL_NSGETMODULE(nsCookieModule)
DECL_NSGETMODULE(nsXMLExtrasModule)
DECL_NSGETMODULE(nsUniversalCharDetModule)
DECL_NSGETMODULE(nsTypeAheadFind)
DECL_NSGETMODULE(TransformiixModule)
DECL_NSGETMODULE(BOOT)
DECL_NSGETMODULE(NSS)
#line 52 "nsStaticComponents.cpp.in"

/**
 * The nsStaticModuleInfo
 */
static nsStaticModuleInfo gStaticModuleInfo[] = {
#define MODULE(_name) { (#_name), NSGETMODULE(_name) }
MODULE(xpcomObsoleteModule),
MODULE(nsI18nModule),
MODULE(nsUConvModule),
MODULE(nsJarModule),
MODULE(xpconnect),
MODULE(necko_core_and_primary_protocols),
MODULE(necko_secondary_protocols),
MODULE(nsPrefModule),
MODULE(nsCJVMManagerModule),
MODULE(nsSecurityManagerModule),
MODULE(nsChromeModule),
MODULE(nsRDFModule),
MODULE(nsParserModule),
MODULE(nsGfxMacModule),
MODULE(nsImageLib2Module),
MODULE(nsPluginModule),
MODULE(nsWidgetMacModule),
MODULE(nsLayoutModule),
MODULE(nsMorkModule),
MODULE(docshell_provider),
MODULE(embedcomponents),
MODULE(Browser_Embedding_Module),
MODULE(nsEditorModule),
MODULE(nsTransactionManagerModule),
MODULE(application),
MODULE(nsCookieModule),
MODULE(nsXMLExtrasModule),
MODULE(nsUniversalCharDetModule),
MODULE(nsTypeAheadFind),
MODULE(TransformiixModule),
MODULE(BOOT),
MODULE(NSS),
#line 60 "nsStaticComponents.cpp.in"
};

/**
 * Our NSGetStaticModuleInfoFunc
 */
nsresult
app_getModuleInfo(nsStaticModuleInfo **info, PRUint32 *count)
{
  *info = gStaticModuleInfo;
  *count = sizeof(gStaticModuleInfo) / sizeof(gStaticModuleInfo[0]);
  return NS_OK;
}

