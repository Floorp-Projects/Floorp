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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsSOAPParameter.h"
#include "nsSOAPUtils.h"
#include "nsIServiceManager.h"
#include "nsISOAPAttachments.h"

nsSOAPParameter::nsSOAPParameter()
{
  NS_INIT_ISUPPORTS();
}

NS_IMPL_CI_INTERFACE_GETTER2(nsSOAPParameter, nsISOAPBlock, nsISOAPParameter)
NS_IMPL_ADDREF_INHERITED(nsSOAPParameter, nsSOAPBlock)
NS_IMPL_RELEASE_INHERITED(nsSOAPParameter, nsSOAPBlock)

NS_INTERFACE_MAP_BEGIN(nsSOAPParameter)
NS_INTERFACE_MAP_ENTRY(nsISOAPParameter)
NS_IMPL_QUERY_CLASSINFO(nsSOAPParameter)
NS_INTERFACE_MAP_END_INHERITING(nsSOAPBlock)

nsSOAPParameter::~nsSOAPParameter()
{
}
