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

#include "IdlAttribute.h"
#include <ostream.h>

ostream& operator<<(ostream &s, IdlAttribute &aAttribute)
{
  if (aAttribute.GetReadOnly()) {
    s << "readonly ";
  }
  s << "attribute ";

  char type[128];
  aAttribute.GetTypeAsString(type, 128);
  return s << type << " " << aAttribute.GetName() << ";";
}

IdlAttribute::IdlAttribute()
{
  mReadOnly = 0;
  mIsNoScript = 0;
  mReplaceable = 0;
}

IdlAttribute::~IdlAttribute()
{
}

void IdlAttribute::SetReadOnly(int aReadOnlyFlag)
{
  mReadOnly = aReadOnlyFlag;
}

int IdlAttribute::GetReadOnly()
{
  return mReadOnly;
}

int             
IdlAttribute::GetIsNoScript()
{
  return mIsNoScript;
}

void            
IdlAttribute::SetIsNoScript(int aIsNoScript)
{
  mIsNoScript = aIsNoScript;
}

int             
IdlAttribute::GetReplaceable()
{
  return mReplaceable;
}

void            
IdlAttribute::SetReplaceable(int aReplaceable)
{
  mReplaceable = aReplaceable;
}
