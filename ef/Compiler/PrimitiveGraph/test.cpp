/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

//#include <iostream.h>
#include "PrimitiveBuilders.h"
#include "Primitives.h"
#include "Pool.h"
#include "ControlGraph.h"

#include "CodeGenerator.h"

void main()
{
  
  Pool* myPool = new Pool(8192, 1024);
  ValueKind* dummyKinds = NULL;
  ControlGraph* myPissAssGraph = new ControlGraph((*myPool), 0, dummyKinds, false); 
  ControlNode* myNode = &myPissAssGraph->newControlNode();

  PrimArg* incoming1 = new PrimArg();
  incoming1->initOutgoingEdge(vkInt, false);

  PrimArg* incoming2 = new PrimArg();
  incoming2->initOutgoingEdge(vkInt, false);

  PrimArg* incoming3 = new PrimArg();
  incoming3->initOutgoingEdge(vkInt, false);

  PrimArg* incoming4 = new PrimArg();
  incoming4->initOutgoingEdge(vkInt, false);

  ProducerOrConstant* inputs= new ProducerOrConstant[2];
  ProducerOrConstant* inputs2= new ProducerOrConstant[2];

  builder_Add_III.specializeConstVar(10, incoming1->getOutgoingEdge(), inputs[0], myNode);
  
	inputs[1].setVar(incoming2->getOutgoingEdge());
	
  builder_Add_III.makeBinaryGeneric(inputs, inputs2[0], myNode);
  
	
	inputs2[1].setVar(incoming3->getOutgoingEdge());

  builder_Add_III.makeBinaryGeneric(inputs2, inputs2[1], myNode);
	

  ProducerOrConstant cond;
  builder_Cmp_IIC.makeBinaryGeneric(inputs2, cond, myNode); 

  


  ControlIf* myIfNode = new ControlIf((*myNode), condLt, cond.getProducer());
                                               
  CodeGenerator engine(*myNode, *myPool);
  
  engine.generate();
}
