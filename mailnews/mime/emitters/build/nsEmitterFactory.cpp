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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "msgCore.h"
#include "nsCOMPtr.h"
#include "pratom.h"

#include "nsIGenericFactory.h"
#include "nsIModule.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsMimeEmitterCID.h"
#include "nsIMimeEmitter.h"
#include "nsMimeHtmlEmitter.h"
#include "nsMimeRawEmitter.h"
#include "nsMimeXmlEmitter.h"
#include "nsMimeXULEmitter.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kHtmlMimeEmitterCID, NS_HTML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kRawMimeEmitterCID, NS_RAW_MIME_EMITTER_CID);
static NS_DEFINE_CID(kXmlMimeEmitterCID, NS_XML_MIME_EMITTER_CID);
static NS_DEFINE_CID(kXULMimeEmitterCID, NS_XUL_MIME_EMITTER_CID);

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeHtmlEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeRawEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeXmlEmitter);
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeXULEmitter);


static nsModuleComponentInfo components[] =
{
  { NS_HTML_MIME_EMITTER_CID, &nsMimeHtmlEmitterConstructor, NS_HTML_MIME_EMITTER_PROGID, },
  { NS_XML_MIME_EMITTER_CID, &nsMimeXmlEmitterConstructor, NS_XML_MIME_EMITTER_PROGID, },
  { NS_RAW_MIME_EMITTER_CID, &nsMimeRawEmitterConstructor, NS_RAW_MIME_EMITTER_PROGID, },
  { NS_XUL_MIME_EMITTER_CID, &nsMimeXULEmitterConstructor, NS_XUL_MIME_EMITTER_PROGID, },
};

NS_IMPL_MODULE(nsMimeEmitterModule, components)
NS_IMPL_NSGETMODULE(nsMimeEmitterModule)
