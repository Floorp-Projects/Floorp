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

#include "iJ2XRETServerTestComponent.h"
#include "ProceedResults.h"
#include "VarContainer.h"

class J2XRETServerTestComponentImpl : public iJ2XRETServerTestComponent
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IJ2XRETSERVERTESTCOMPONENT
    DECL_PROCEED_RESULTS
    DECL_VAR_STACKS

    J2XRETServerTestComponentImpl();
    virtual ~J2XRETServerTestComponentImpl();

};


class _MYCLASS_ : public iJ2XRETServerTestComponent
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IJ2XRETSERVERTESTCOMPONENT

  _MYCLASS_();
  virtual ~_MYCLASS_();
  /* additional members */
};










