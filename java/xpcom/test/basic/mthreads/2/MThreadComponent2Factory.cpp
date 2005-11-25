/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/

#include "nsIGenericFactory.h"

#include "MThreadComponent2.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(MThreadComponent2Impl)

static nsModuleComponentInfo components[] =
{
  { "MultiThread Component 2", MTHREADCOMPONENT2_CID, MTHREADCOMPONENT2_PROGID, MThreadComponent2ImplConstructor,
     NULL /* NULL if you don't need one */,
     NULL /* NULL if you don't need one */
  }
};

NS_IMPL_NSGETMODULE("MThreadComponent2Factory", components)
