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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Client QA Team, St. Petersburg, Russia
 */

#include "prio.h"
#include "prprf.h"

#define DECL_PSEUDOHASH \
class PseudoHash\
{\
 public:\
	PseudoHash();\
	~PseudoHash();\
	void put(char*);\
	PRBool containsKey(char*);\
};\

#define IMPL_PSEUDOHASH(_MYCLASS_) \
 PseudoHash::PseudoHash() {\
 }\
 PseudoHash::~PseudoHash() {\
 }\
 void PseudoHash::put(char* name) {\
  fprintf(stdout,"Debug:ovk:PseudoHash::put::%s\n",name);\
  hash[hashCount]=name;\
 }\
 PRBool PseudoHash::containsKey(char* name) {\
  for(int i=0;i<hashCount;i++) {\
    fprintf(stdout,"Debug:ovk:PseudoHash::containsKey::NAME=%s HASH=%s\n",name,hash[i]);\
    if (PL_strcmp(name,hash[i])==0) return PR_TRUE;\
  }\
  return PR_FALSE;\
 }
