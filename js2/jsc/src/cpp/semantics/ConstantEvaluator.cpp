
#pragma warning ( disable : 4786 )

#include "Nodes.h"
#include "ConstantEvaluator.h"
#include "NodeFactory.h"
#include "ReferenceValue.h"
#include "TypeValue.h"
#include "GlobalObjectBuilder.h"

namespace esc {
namespace v1 {

    // Base node
    
	Value* ConstantEvaluator::evaluate( Context& cx, Node* node ) { 
        throw;
    }

	/*
     * Unqualified identifier
     *
     * STATUS
     *
     * NOTES
     * An unqualified name can bind to a local variable, instance
     * property, or global property. A local will result in a
     * aload_n instruction. A instance property will result in a 
     * get_property method call. A global property will result in a
     * get_property method call on the global object.
     */

	Value* ConstantEvaluator::evaluate( Context& cx, IdentifierNode* node ) { 
        return new ReferenceValue((ObjectValue*)0,node->name,used_namespaces);
    }

    /*
     * QualifiedIdentifierNode
     *
     * {qualifiers:QualifierListNode name:String}
     *
     * Semantics:
     * 1. Evaluate qualifier.
     * 2. Call GetValue(Result(1).
     * 3. If Result(2) is not a namespace, then throw a TypeError.
     * 4. Create a new Reference(null,Result2),name).
     * 5. Return Result(4).
     */

	Value* ConstantEvaluator::evaluate( Context& cx, QualifiedIdentifierNode* node ) { 

		Value* qualifier;
		if( node->qualifier != (Node*)0 ) {
			qualifier = node->qualifier->evaluate(cx,this);
			qualifier = qualifier->getValue(cx);
		}

		if( qualifier == (Value*)0 ) {
			//error();
			throw;
		}
		else
		if(!(TypeValue::namespaceType->includes(cx,qualifier)) ) {
			//error();
			throw;
		}

		return new ReferenceValue((ObjectValue*)0,node->name,qualifier);
    }
    
	/*
	 * CallExpression
	 *
	 * Semantics
	 * ---------
	 * 1 Evaluate MemberExpression.
	 * 2 Evaluate Arguments, producing an internal list of argument 
	 *   values (section 11.2.4).
	 * 3 Call GetValue(Result(1)).
	 * 4 If Type(Result(3)) is not Object, throw a TypeError exception.
	 * 5 If Result(3) does not implement the internal [[Call]] method, 
	 *   throw a TypeError exception.
	 * 6 If Type(Result(1)) is Reference, Result(6) is GetBase(Result(1)). 
	 *   Otherwise, Result(6) is null.
	 * 7 If Result(6) is an activation object, Result(7) is null. 
	 *   Otherwise, Result(7) is the same as Result(6).
	 * 8 Call the [[Call]] method on Result(3), providing Result(7) 
	 *   as the this value and providing the list Result(2) as the 
	 *   argument values.
	 * 9 Return Result(8).
	 * 
	 */
	
	Value* ConstantEvaluator::evaluate( Context& cx, CallExpressionNode* node ) { 

		Value* val = node->member->evaluate(cx,this);
		ReferenceValue* ref = dynamic_cast<ReferenceValue*>(val); // Forces the use of RTTI.
		
		if( node->args ) {
			node->args->evaluate(cx,this);
		}

		// Do typechecking. It is quite possible that this function has not 
		// yet been defined. In this case, all of the semantic actions must 
		// be performed at runtime. 

		if( ref->lookup(cx) ) {
			
			/* If the function was found: The slot index is in 
			 * ref->slot_index and the base object is presumed 
			 * to be the top value on the operand stack (if base 
			 * is set), otherwise it is the nth (ref->scope_index) 
			 * object from the bottom of the scope stack. (0 is
			 * global.)
			 *
			 * Nothing to do here.
			 */
		}

		node->ref = ref;

		return ObjectValue::undefinedValue;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, GetExpressionNode* node ) { 

		Value* val = node->member->evaluate(cx,this);
		ReferenceValue* ref = dynamic_cast<ReferenceValue*>(val); // Forces the use of RTTI.
		ref->name.insert(0,"get "); // Tweak the name to make it a getter reference.

		// Do typechecking. It is quite possible that this function has not 
		// yet been defined. In this case, all of the semantic actions must 
		// be performed at runtime. 

		if( ref->lookup(cx) ) {
			
			/* If the function was found: The slot index is in 
			 * ref->slot_index and the base object is presumed 
			 * to be the top value on the operand stack (if base 
			 * is set), otherwise it is the nth (ref->scope_index) 
			 * object from the bottom of the scope stack. (0 is
			 * global.)
			 *
			 * Nothing to do here.
			 */

		}

		return node->ref = ref;
	}

	Value* ConstantEvaluator::evaluate( Context& cx, SetExpressionNode* node ) { 
		return 0;
	}

    Value* ConstantEvaluator::evaluate( Context& cx, ThisExpressionNode* node ) { 
		throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralBooleanNode* node ) { 
        throw;
    }

    Value* ConstantEvaluator::evaluate( Context& cx, LiteralNumberNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralStringNode* node ) { 
        return ObjectValue::undefinedValue;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralUndefinedNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralRegExpNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, UnitExpressionNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FunctionExpressionNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ParenthesizedExpressionNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ParenthesizedListExpressionNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralObjectNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralFieldNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LiteralArrayNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, PostfixExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, NewExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, IndexedMemberExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ClassofExpressionNode* node ) {
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, MemberExpressionNode* node ) { 

		/* If the member expression has a base, evaluate it and try to
		 * bind it to its slot. If there it binds, then get the slot
		 * type and then the prototype of that type. The prototype will
		 * be the compile-time stand-in for the instance.
		 */
		ObjectValue* prototype = 0;
		if( node->base ) {
			Value*          val  = node->base->evaluate(cx,this);
			ReferenceValue* ref  = dynamic_cast<ReferenceValue*>(val); // Forces the use of RTTI.
			Slot*           slot = ref->getSlot(cx);
			TypeValue*      type = slot->type;
			
			prototype = type->prototype;
		}

		ReferenceValue* ref  = dynamic_cast<ReferenceValue*>(node->expr->evaluate(cx,this));

		ref->setBase(prototype);

		return ref;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, CoersionExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, UnaryExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, BinaryExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ConditionalExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, AssignmentExpressionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ListNode* node ) { 
		if( node->list ) {
			node->list->evaluate(cx,this); 
		} 
		if( node->item ) {
			node->item->evaluate(cx,this);
		}
		return ObjectValue::undefinedValue;
    }

    // Statements

    Value* ConstantEvaluator::evaluate( Context& cx, StatementListNode* node ) { 
		if( node->list ) {
			node->list->evaluate(cx,this); 
		} 
		if( node->item ) {
			node->item->evaluate(cx,this);
		}
		return ObjectValue::undefinedValue;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, EmptyStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ExpressionStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, AnnotatedBlockNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, LabeledStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, IfStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, SwitchStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, CaseLabelNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, DoStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, WhileStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ForInStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ForStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, WithStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ContinueStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, BreakStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ReturnStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ThrowStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, TryStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, CatchClauseNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FinallyClauseNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, UseStatementNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, IncludeStatementNode* node ) { 
        throw;
    }

    // Definitions

    Value* ConstantEvaluator::evaluate( Context& cx, ImportDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ImportBindingNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, AnnotatedDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, AttributeListNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ExportDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ExportBindingNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, VariableDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, VariableBindingNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, TypedVariableNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FunctionDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FunctionDeclarationNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FunctionNameNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, FunctionSignatureNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ParameterNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, OptionalParameterNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ClassDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, ClassDeclarationNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, InheritanceNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, NamespaceDefinitionNode* node ) { 
        throw;
    }
    
	Value* ConstantEvaluator::evaluate( Context& cx, PackageDefinitionNode* node ) { 
        throw;
    }

    Value* ConstantEvaluator::evaluate( Context& cx, ProgramNode* node ) {
        if( node->statements != (ProgramNode*)0 ) {
			node->statements->evaluate(cx,this); 
		}
        return (Value*)0;
    }

    /*
     * Test driver
     */

    int ConstantEvaluator::main(int argc, char* argv[]) {

		Context cx;
		Evaluator* evaluator = new ConstantEvaluator();

		Builder*     globalObjectBuilder = new GlobalObjectBuilder();
		ObjectValue* globalPrototype     = new ObjectValue(*globalObjectBuilder);
		ObjectValue  global              = ObjectValue(globalPrototype);

		cx.pushScope(&global); // first scope is always considered the global scope.
		cx.used_namespaces.push_back(ObjectValue::publicNamespace);

		Node* node;
		Value* val;

		// public::print
		
		node = NodeFactory::QualifiedIdentifier(
					NodeFactory::Identifier("public"),
					NodeFactory::Identifier("print"));
		val  = node->evaluate(cx,evaluator);
		val  = val->getValue(cx);

		// get version

		node = NodeFactory::GetExpression(
			NodeFactory::MemberExpression(0,
				NodeFactory::Identifier("version")));

		val  = node->evaluate(cx,evaluator);
		val  = val->getValue(cx);

		// print('hello')

		node = NodeFactory::CallExpression(NodeFactory::MemberExpression(0,NodeFactory::Identifier("print")),0);
		val  = node->evaluate(cx,evaluator);
		val  = val->getValue(cx);

		return 0;
    }

}
}

/*
 * Written by Jeff Dyer
 * Copyright (c) 1998-2001 by Mountain View Compiler Company
 * All rights reserved.
 */
