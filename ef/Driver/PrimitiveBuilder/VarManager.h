/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _VAR_MANAGER_H_
#define _VAR_MANAGER_H_

#include "Fundamentals.h"
#include "SymbolManager.h"
#include "Primitives.h"
#include "Pool.h"

class VarManager : public SymbolManager<DataNode, DataConsumer>
{
private:
  Pool& ePool;

public:
  VarManager(Pool& envPool) : SymbolManager<DataNode, DataConsumer>(envPool), ePool(envPool) {}

  inline void link(DataNode& producer, DataConsumer& consumer) 
    {
      if (consumer.getNode()->hasCategory(pcPhi))
		{
		  PhiNode *phiNode = (PhiNode *)consumer.getNode();
		  phiNode->addInput(producer, ePool);
		}
      else
		consumer.setVar(producer);
	}
};

#endif /* _VAR_MANAGER_H_ */
