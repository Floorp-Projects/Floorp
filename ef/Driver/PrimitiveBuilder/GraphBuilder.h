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

#ifndef _GRAPH_BUILDER_H_
#define _GRAPH_BUILDER_H_

#include <string.h> // for strdup
#include <stdlib.h> // for free
#include "ControlGraph.h"
#include "Fundamentals.h"
#include "Value.h"
#include "VarManager.h"
#include "LabelManager.h"
#include "Pool.h"

class ConsumerOrConstant
{
private:
  union
  {
	char  *name;
	Value value;
  };
  bool is_constant;

public:
  ConsumerOrConstant(char *name) : is_constant(false), name(strdup(name)) {}
  ConsumerOrConstant(const Value& value) : is_constant(true), value(value) {}
  ~ConsumerOrConstant() {free(name);}

  Value& getValue() {return value;}
  char *getName() {return name;}
  bool isConstant() {return is_constant;}
};

#define akVariable 0x01
#define akConstant 0x02
#define akAny  (akVariable | akConstant)

class GraphBuilder
{
private:

  Pool& envPool;
  VarManager varManager;
  LabelManager labelManager;

  ControlGraph* graph;
  ControlNode* currentNode;
  ControlEdge* predecessorEdge;
  char *lastCondition;

public:
  GraphBuilder(Pool& envPool) : envPool(envPool), varManager(envPool), labelManager(envPool) {}
  ControlGraph& parse(char *filename);


  void checkInputArgument(Vector<ConsumerOrConstant *>& in, uint32 pos, uint8 kind);
  void checkOutputArgument(char *name);
  void registerProducer(char *name, DataNode& producer);
  void connectConsumer(char *name, DataConsumer& consumer);
  void buildConstPrimitive(Value& value, ValueKind constantKind, char *producer);
  void buildGenericBinaryPrimitive(PrimitiveOperation op, ValueKind outputKind, ConsumerOrConstant& arg0, ConsumerOrConstant& arg1, char *producer);
  void buildIfPrimitive(PrimitiveOperation op, DataNode& cond, char *branch_to_if_true);
  void buildPrimitive(PrimitiveOperation op, Vector<ConsumerOrConstant *>& in, char *out = 0);
  void buildPhiNode(Vector<ConsumerOrConstant *>& in, ValueKind kind, char *producer);
  void buildArgPrimitive(ValueKind outputKind, ConsumerOrConstant& arg0, char *producer);
  void buildResultPrimitive(ValueKind kind, ConsumerOrConstant& arg0);
  void buildStorePrimitive(ValueKind kind, ConsumerOrConstant& arg0, ConsumerOrConstant& arg1);
  char *getNewConditionName();
  void defineLabel(char *name);
  void finishCurrentNode(char *successor);
  void undefinedSymbolError(Vector<char *>& symbols);
  char *getPhiVariable(Vector<ConsumerOrConstant *>& vars, char *label = 0);
  void aliasVariable(char *var, char* alias); 
  void generateCode(); // FIXME: to remove
};

#endif /* _GRAPH_BUILDER_H_ */
