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

#include "X2JRETClientTestComponent.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(X2JRETClientTestComponentImpl)

static nsModuleComponentInfo components[] =
{
 {"X2JRET Client Test Component",
   X2JRETCLIENTTESTCOMPONENT_CID,
   X2JRETCLIENTTESTCOMPONENT_PROGID,
   X2JRETClientTestComponentImplConstructor,
   NULL /* NULL if you don't need one */,
   NULL /* NULL if you don't need one */
 }
};

NS_IMPL_NSGETMODULE("X2JRETClientTestComponentFactory", components)
