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

#include "Fundamentals.h"
#include "ControlGraph.h"
#include "Primitives.h"
#include "GraphBuilder.h"
#include "Pool.h"
#include "cstubs.h"
#include "Parser.h"

ControlGraph& GraphBuilder::
parse(char *filename)
{
  if (freopen(filename, "r", stdin) == NULL)
	{
	  perror("");
	  exit(EXIT_FAILURE);
	}

  YYSTYPE yylval;
  int yychar;
  uint32 argCount = 0;
  while ((yychar = yylex(&yylval)) > 0)
	{
	  if ((yychar == PRIMITIVE) && (yylval.yyPrimitiveOperation == poArg_I))
		argCount++;
	}

  freopen(filename, "r", stdin);

  ValueKind* kinds = new(envPool) ValueKind[argCount];
  for (uint32 i = 0; i < argCount; i++)
	kinds[i] = vkInt;

  graph = new(envPool) ControlGraph(envPool, argCount, kinds, 1);

  predecessorEdge = 0;
  lastCondition = 0;
  currentNode = &graph->newControlNode();

  yyparse(this);

  // link to the end node.
  finishCurrentNode("__end__");
  currentNode->remove();

  if (!labelManager.used("__end__"))
	yyerror("no end to this method");

  // link the begin node to the start label.
  labelManager.resolve("start", graph->getBeginExtra().getSuccessorEdge());
  labelManager.define("__end__", graph->getEndNode());

  // Initialize the finalMemory in the EndNode.
  graph->getEndExtra().finalMemory.getInput().
	setVar(graph->getBeginExtra().initialMemory.getOutgoingEdge());

  // check for link errors
  if (varManager.hasUndefinedSymbols())
	undefinedSymbolError(varManager.undefinedSymbols());
  if (labelManager.hasUndefinedSymbols())
	undefinedSymbolError(labelManager.undefinedSymbols());

  return *graph;
}

void GraphBuilder::
aliasVariable(char *var, char* alias)
{
  assert(varManager.defined(var));
  varManager.define(alias, varManager.get(var));
}

char *GraphBuilder::
getPhiVariable(Vector<ConsumerOrConstant *>& vars, char *label)
{
  uint32 str_size = 0;
  char *phi_variable, *ptr;
  bool restore = false;
  ControlNode* saved_node;

  if (label)
	if (labelManager.defined(label))
	  {
		saved_node = currentNode;
		currentNode = &labelManager.get(label);
		restore = true;
	  }
	else
	  {
		fprintf(stderr, "error: Label %s is not yet defined.", label);
		exit(EXIT_FAILURE);
	  }

  for (uint32 i = 0; i < vars.size(); i++)
	{
	  checkInputArgument(vars, i, akVariable);
	  str_size += (strlen(vars[i]->getName()) + 1);
	}
  str_size += 3 /* phi */ + 1 /* '\0' */;
  phi_variable = (char *) malloc(str_size);
  ptr = phi_variable;
  sprintf(ptr, "phi"); ptr += 3;
  for (uint32 j = 0; j < vars.size(); j++)
	{
	  sprintf(ptr, "_%s", vars[j]->getName()); ptr += strlen(ptr);
	}

  if (!varManager.defined(phi_variable))
	{
	  buildPhiNode(vars, vkInt, phi_variable);
	}
  else
	{
	  DataNode& phi_producer = varManager.get(phi_variable);
	  DoublyLinkedList<PhiNode>& phiNodes = currentNode->getPhiNodes();
	  bool found = false;

	  for (DoublyLinkedList<PhiNode>::iterator it = phiNodes.begin(); !phiNodes.done(it);
		   it = phiNodes.advance(it))
		if (&phiNodes.get(it).getOutgoingEdge() == &phi_producer)
		  {
			found = true;
			break;
		  }
	  if (!found)
		{
		  fprintf(stderr, "error: the phi node at line %d was defined in another block.\n"
				  "       Try phi(arg, ...)@label\n", yylineno);
		  exit(EXIT_FAILURE);
		}
	}

  if (restore)
	currentNode = saved_node;

  return phi_variable;
}

void GraphBuilder::
undefinedSymbolError(Vector<char *>& symbols)
{
  uint32 count = symbols.size();
  fprintf(stderr, "error: undefined symbol%s: ", count > 1 ? "s" : "");
  for (uint32 i=0; i < count; i++)
	fprintf(stderr, "%s ", symbols[i]);
  fprintf(stderr, "\n");
  exit(EXIT_FAILURE);
}

void GraphBuilder::
checkInputArgument(Vector<ConsumerOrConstant *>& in, uint32 pos, uint8 kind)
{
  if (pos >= in.size())
	yyerror("wrong input argument count");
  else if (!((in[pos]->isConstant() ? akConstant : akVariable) & kind))
	yyerror("wrong argument kind");
}

void GraphBuilder::
checkOutputArgument(char *name)
{
  if (!name)
	yyerror("need an output argument");
}

void GraphBuilder::
connectConsumer(char *name, DataConsumer& consumer)
{
  varManager.resolve(name, consumer);
}

void GraphBuilder::
registerProducer(char *name, DataNode& producer)
{
  if (varManager.defined(name))
	{
	  char fmt[] = "duplicate variable definition %s";
	  char* error = (char *) malloc(strlen(fmt) - 2 + strlen(name) + 1);
	  sprintf(error, fmt, name);
	  yyerror(error);
	}
  varManager.define(name, producer);
}

void GraphBuilder::
buildConstPrimitive(Value& value, ValueKind constantKind, char *producer)
{
  PrimConst& primConst = *new(envPool) PrimConst(constantKind, value);
  registerProducer(producer, primConst.getOutgoingEdge());
  currentNode->addPrimitive(primConst);
}

void GraphBuilder::
buildArgPrimitive(ValueKind outputKind, ConsumerOrConstant& arg0, char *producer)
{
  ValueKind v = outputKind; v = vkInt;
  uint32 argNum = arg0.getName()[strlen(arg0.getName())-1] - '0';

  PrimArg& primArg = graph->getBeginExtra().arguments[argNum];
  char buffer[] = "arg0";
  buffer[3] = '0' + argNum;
  registerProducer(buffer, primArg.getOutgoingEdge());
  registerProducer(producer, primArg.getOutgoingEdge());
}

void GraphBuilder::
buildResultPrimitive(ValueKind kind, ConsumerOrConstant& arg0)
{
  ValueKind* kinds = new(envPool) ValueKind[1];
  kinds[0] = kind;
  ControlReturn& block = *new(envPool) ControlReturn(*currentNode, 1, kinds, envPool);
  connectConsumer(arg0.getName(), block.results[0].getInput());
  if (predecessorEdge)
	currentNode->addPredecessor(*predecessorEdge);
  predecessorEdge = &block.getSuccessorEdge();

  currentNode = &graph->newControlNode();
}

void GraphBuilder::
buildStorePrimitive(ValueKind /*outputKind*/, ConsumerOrConstant& /*arg0*/, ConsumerOrConstant& /*arg1*/)
{
}

void GraphBuilder::
buildGenericBinaryPrimitive(PrimitiveOperation op, ValueKind outputKind, 
							ConsumerOrConstant& arg0, ConsumerOrConstant& arg1, char *producer)
{
  PrimBinaryGeneric& prim = *new(envPool) PrimBinaryGeneric(op, outputKind, true);
  if (arg0.isConstant())
	prim.getInputs()[0].setConst(arg0.getValue().i);
  else
	connectConsumer(arg0.getName(), prim.getInputs()[0]);

  if (arg1.isConstant())
	prim.getInputs()[1].setConst(arg1.getValue().i);
  else
	connectConsumer(arg1.getName(), prim.getInputs()[1]);

  registerProducer(producer, prim.getOutgoingEdge());
  currentNode->addPrimitive(prim);
}

void GraphBuilder::
buildIfPrimitive(PrimitiveOperation op, DataNode& cond, char *branch_to_if_true)
{
  // finish the Block
  ControlIf& block = *new(envPool) ControlIf(*currentNode, Condition2(condLt + op - poIfLt), cond);
  if (predecessorEdge)
	currentNode->addPredecessor(*predecessorEdge);
  predecessorEdge = &block.getFalseIfEdge();
  labelManager.resolve(branch_to_if_true, block.getTrueIfEdge());

  currentNode = &graph->newControlNode();
}

void GraphBuilder::
finishCurrentNode(char *successor)
{
  if (!currentNode->getPrimitives().empty())
	{
	  ControlBlock& cb = *new(envPool) ControlBlock(*currentNode);
	  if (predecessorEdge)
		currentNode->addPredecessor(*predecessorEdge);
	  predecessorEdge = 0;
	  labelManager.resolve(successor, cb.getSuccessorEdge());
	  currentNode = &graph->newControlNode();
	}
  else
	{
	  if (predecessorEdge)
		labelManager.resolve(successor, *predecessorEdge);
	  predecessorEdge = 0;
	}
}

char *GraphBuilder::
getNewConditionName()
{
  static uint32 index = 0;
  static char buffer[4 + 3 + 1];
  sprintf(buffer, "cond%3.3d", index++);
  return buffer;
}

void GraphBuilder::
buildPhiNode(Vector<ConsumerOrConstant *>& in, ValueKind kind, char *producer)
{
  uint32 count = in.size();
  PhiNode& phiNode = *new(envPool) PhiNode(count, kind, envPool);
  DataConsumer& fakeConsumer = *new(envPool) DataConsumer();
  fakeConsumer.setNode(&phiNode);

  for (uint32 i = 0; i < count; i++)
	{
	  checkInputArgument(in, i, akVariable);
	  connectConsumer(in[i]->getName(), fakeConsumer);
	}
  registerProducer(producer, phiNode.getOutgoingEdge());
  currentNode->addPhiNode(phiNode);
}

void GraphBuilder::
buildPrimitive(PrimitiveOperation op, Vector<ConsumerOrConstant *>& in, char *out)
{
  switch(op)
	{
	case poConst_I:
	  checkInputArgument(in, 0, akConstant);
	  checkOutputArgument(out);
	  buildConstPrimitive(in[0]->getValue(), vkInt, out);
	  break;

	case poArg_I:
	  checkInputArgument(in, 0, akVariable);
	  checkOutputArgument(out);
	  buildArgPrimitive(vkInt, *in[0], out);
	  break;

	case poResult_I:
	  checkInputArgument(in, 0, akVariable);
	  buildResultPrimitive(vkInt, *in[0]);
	  break;

	case poSt_I:
	  checkInputArgument(in, 0, akVariable);
	  checkInputArgument(in, 1, akVariable);
	  buildStorePrimitive(vkInt, *in[0], *in[1]);
	  break;

	case poAdd_I:
	case poSub_I:
	case poMul_I:
	case poAnd_I:
	case poOr_I:
	case poXor_I:
	  checkInputArgument(in, 0, akAny);
	  checkInputArgument(in, 1, akAny);
	  checkOutputArgument(out);

	  buildGenericBinaryPrimitive(op, vkInt, *in[0], *in[1], out);
	  break;

	case poCmp_I:
	  checkInputArgument(in, 0, akAny);
	  checkInputArgument(in, 1, akAny);
	  lastCondition = (out) ? out : getNewConditionName();
	  buildGenericBinaryPrimitive(poCmp_I, vkCond, *in[0], *in[1], lastCondition);
	  break;

	case poIfLt:
	case poIfEq:
	case poIfLe:
	case poIfGt:
	case poIfNe:
	case poIfGe:
	  checkInputArgument(in, 0, akVariable); // label name or cond.
	  if (in.size() == 1)
		{
		  if (!varManager.exists(lastCondition))
			yyerror("no condition set for this control");
		  buildIfPrimitive(op, varManager.get(lastCondition), in[0]->getName());
		}
	  else
		{
		  checkInputArgument(in, 1, akVariable); // label name.
		  lastCondition = in[0]->getName();
		  buildIfPrimitive(op, varManager.get(lastCondition), in[1]->getName());		  
		}
	  break;

	case poPhi_I:
	  checkOutputArgument(out);
	  buildPhiNode(in, vkInt, out);
	  break;

	case nPrimitiveOperations: // bra
	  checkInputArgument(in, 0, akVariable); // label name.
	  finishCurrentNode(in[0]->getName());
	  break;

	default:
	  yyerror("unhandled primitive operation");
	  break;
	}
}

void GraphBuilder::
defineLabel(char *name)
{
  if (!currentNode->getPrimitives().empty())
	{
	  // there is an incoming edge
	  ControlBlock& cb = *new(envPool) ControlBlock(*currentNode);
	  if (predecessorEdge)
		currentNode->addPredecessor(*predecessorEdge);
	  predecessorEdge = &cb.getSuccessorEdge();
	  currentNode = &graph->newControlNode();
	}
  labelManager.define(name, *currentNode);
}
