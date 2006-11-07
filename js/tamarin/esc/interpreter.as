/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

package es4 
{
    import avmplus.Domain
    import flash.utils.ByteArray

    namespace avm        // interpret using the avm
    namespace ast        // interpret ast directly
    namespace shared    // common interpreter routines

	function countParams(slots)
	{
		var count = 0
		for each( var slot in slots )
		{
			if( slot.@is_param != void 0 )
			{
				++count
			}
		}
		return count
	}

	function countLocals(slots)
	{
		var count = slots.length()+1 // +1 for 'this'
		return count
	}

    class Interpreter
    {
        use namespace release
        use namespace shared
        use namespace avm
        //use namespace ast

        avm var emitter : ABCEmitter = new ABCEmitter()
        ast var emitter // unused
        avm var state // unused
        ast var state = {stack:[]}
        
        // entry point

        function interpretProgram(node)
        {
            return interpProgram(node)
        }

        // INTERPERTER ROUTINES

        shared function interpPrologue(node,slot_infos)
        {
            Debug.enter("Prologue",node)

            for each ( var slot in node.Slot )
            {
                interpSlot(slot_infos,slot)
            }
            var result = slot_infos

            Debug.exit("Prologue",result,result.length)
            return result
        }

        shared function getScopeDepth(node) 
        {
            var parent = node
            var scope_depth : int = 0 

            do
            {
                if( node.name() == "Block" )
                {
                    scope_depth++
                }
                parent = parent.parent()
            }
            while( parent.name() != "Program" )
            return scope_depth
        }

        /*

        The qualified name of an unqualified type annotation is known at compile time.
        That is, the binding for the reference is known. So at this point, we assume
        that the type name has been rewritten as a qualified name.
        
        */
        
        avm function interpSlot(slot_infos,node)
        {
            Debug.enter("Slot",node)

            var result = slot_infos.length
            var name = node.Name.QualifiedIdentifier.Identifier.@name
            var namespace = node.Name.QualifiedIdentifier.Qualifier.Namespace
            var tname = node.Type.QualifiedIdentifier.Identifier.@name
            var tnamespace = node.Type.QualifiedIdentifier.Qualifier.Namespace

			var slot_id = 0 // auto assign

            emitter.SlotInfo(slot_infos,0,{name:name,namespace:namespace},{name:tname,namespace:tnamespace},slot_id)

            if( node.@kind == "class" )
            {
                emitter.FindProperty(name,[namespace],false,false,false)
                interpClass(node.Value.Class)   // class object on stack
                emitter.SetProperty(name,[namespace],false,false,false,false)
            }
            else
            if( node.@kind == "function" )
            {
                emitter.FindProperty(name,[namespace],false,false,false)
                interpFunction(node.Value.Function)
                emitter.SetProperty(name,[namespace],false,false,false,false)
            }
            else
            if( node.@kind == "var" )
            {
                var scope_depth = getScopeDepth(node)
                if( node.Value != undefined )
                {
                    emitter.FindProperty(name,[namespace],false,false,false)
                    interpExpression(node.Value.*)
                    emitter.SetProperty(name,[namespace],false,false,false,false)
                }
            }

            Debug.exit("Slot",result)
            return result
        }

        ast function interpSlot(slot_infos,node)
        {
            Debug.enter("Slot",node)

            var result

            Debug.exit("Slot",result)
            return result
        }

        shared function interpBlock(node,slot_infos)
        {
            Debug.enter("Block",node)
            
            for each( var n in node.* )
            {                
                var name : String = n.localName()
                switch(name)
                {
                    case "BlockStatement":
                        var result = interpBlockStatement(n)
                        break
                    case "ExpressionStatement":
                        var result = interpExpressionStatement(n)
                        break
                    case "Return":
                        var result = interpReturnStatement(n)
                        break
                    default:
                        throw "Block not implemented for "+node
                }
            }

            Debug.exit("Block",result)
            return result
        }

        // SCOPING FEATURES

        /*
        
        Program

        */

        avm function interpProgram(node)
        {
            Debug.enter("Program",node)

            var name = "$init"
            emitter.StartProgram()
            emitter.StartMethod(name)    // push the current body and create a new one on the body stack
            emitter.LoadThis()
            emitter.PushScope()     // push the global object
            emitter.PushUndefined()

            var slot_infos = []
            interpPrologue(node.Prologue,slot_infos)
            interpBlock(node.Block,slot_infos)

            emitter.Return()
			var locals_count = countLocals(node.Prologue.Slot)
            var method_index = emitter.FinishMethod(name,[]/*slot_infos*/,false,0,locals_count)   // return the index of the created method 

            var init_index = emitter.methodCount() - 1
            emitter.FinishProgram(init_index,slot_infos)

            // Emit bytes

            var bytes = emitter.emit()
            bytes.writeFile("temp.abc")
            Debug.log_mode::trace("temp.abc, "+bytes.length+" bytes written")

            // Execute bytes

            var domain = Domain.currentDomain
            var result = <Value>{domain.loadBytes(bytes)}</Value>

            Debug.exit("Program",result)
            return result
        }

        ast function interpProgram(node)
        {
            Debug.enter("Program",node)

            var slot_infos = []
            interpPrologue(node.Prologue,slot_infos)
            interpBlock(node.Block,slot_infos)

            if( state.stack.length != 1 )
            {
                throw "final stack depth is " + state.stack.length
            }
            var result = state.stack[0]

            Debug.exit("Program",result)
            return result
        }

        /*

        Block statement

        */

        avm function interpBlockStatement(node)
        {
            Debug.enter("BlockStatement",node)

            var name = node.@kind.toString() == "package" ? node.@name+"$block" : 
                        node.@kind.toString() == "let" ? getUniqueName("let$block"): getUniqueName("block")

            emitter.StartMethod(name)
            emitter.NewActivation()
            emitter.PushScope()
            emitter.PushUndefined()

            var slot_infos = []
            interpPrologue(node.Prologue,slot_infos)
            interpBlock(node.Block,slot_infos)

            emitter.Return()
			var locals_count = countLocals(node.Prologue.Slot)
            var method_index = emitter.FinishMethod(name,slot_infos,false,0,locals_count)   // return the index of the created method 

            emitter.NewFunction(method_index)
            emitter.GetGlobalScope()
            emitter.Call(0)
            emitter.Pop() // remove the result of call from stack

            Debug.exit("BlockStatement",result)
            return result
        }

        ast function interpBlockStatement(node)
        {
            Debug.enter("BlockStatement",node)

            Debug.exit("BlockStatement",result)
            return result
        }

        /*

        Class

        */

        avm function interpClass(node)
        {
            Debug.enter("Class",node)

            var name : String = node.ClassName.Identifier.@name + "$cinit"
            emitter.StartMethod(name)    // cinit
            emitter.NewActivation()
            emitter.Dup()  // copy of activation object returned by cinit
            emitter.PushScope()

            var slot_infos = []
            interpPrologue(node.Prologue,slot_infos)
            interpBlock(node.Block,slot_infos)

            emitter.Return()

			var locals_count = countLocals(node.Prologue.Slot)
            var method_index = emitter.FinishMethod(name,slot_infos,false,0,locals_count)   // return the index of the created method 

            // Create new function and invoke it

            emitter.NewFunction(method_index)
            emitter.GetGlobalScope()
            emitter.Call(0)

            Debug.exit("Class",result)
            return result
        }

        ast function interpClass(node)
        {
            Debug.enter("Class",node)

            // TBD

            Debug.exit("Class",result)
            return result
        }

        /*

        Function

        */

        avm function interpFunction(node)
        {
            Debug.enter("Function",node)

            var name : String = node.@name!=void 0?node.@name:getUniqueName("function")
            emitter.StartMethod(name)

            emitter.NewActivation()
            emitter.PushScope()

            var slot_infos = []
            interpPrologue(node.Prologue,slot_infos)

			var param_count = countParams(node.Prologue.Slot)
			var scope_depth = getScopeDepth(node)
			var has_arguments = node.@has_arguments=="true"
			var implicit_param_count = has_arguments ? 1 : 0
			
			for( var i = 1; i < param_count+implicit_param_count; i++ )  // make sure we copy value of 'arguments' too
			{
				emitter.GetScopeObject(scope_depth)
				emitter.GetLocal(i)
				emitter.SetSlot(i)
			}

            interpBlock(node.Block,slot_infos)
			emitter.PopScope()

			// todo: use cfa to determine if this is necessary
            emitter.PushUndefined() // return undefined if we run off the end
            emitter.Return()  // if we run off the end

			var locals_count = countLocals(node.Prologue.Slot)
            var method_index = emitter.FinishMethod(name,slot_infos,has_arguments,param_count,locals_count)   // return the index of the created method 

            emitter.NewFunction(method_index)  // leave new function on stack

            Debug.exit("Function",result)
            return result
        }

        ast function interpFunction(node)
        {
            Debug.enter("Function",node)

            // TBD

            Debug.exit("Function",result)
            return result
        }


        // STATEMENTS

        /*

        Expression statement

        */

        avm function interpExpressionStatement(node)
        {
            Debug.enter("ExpressionStatement",node)

            const needs_result = false
            var result = interpExpression(node.*,needs_result)

            Debug.exit("ExpressionStatement",result)
            return result
        }

        ast function interpExpressionStatement(node)
        {
            Debug.enter("ExpressionStatement",node)

            const needs_result = false
            var result = interpExpression(node.*,needs_result)

            Debug.exit("ExpressionStatement",result)
            return result
        }

        /*

        Return statement

        */

        avm function interpReturnStatement(node)
        {
            Debug.enter("ReturnStatement",node)

            const needs_result = true
            var result = interpExpression(node.*,needs_result)
            emitter.Return()

            Debug.exit("ReturnStatement",result)
            return result
        }

        ast function interpReturnStatement(node)
        {
            Debug.enter("ReturnStatement",node)

            // tbd

            Debug.exit("ReturnStatement",result)
            return result
        }

        // EXPRESSIONS

        shared function makeNamespaceSet(node)
        {
            var result = []
            for each ( var item in node.Namespace )
            {
                result.push(item.@name)
            }
            return result            
        }

        shared function interpQualifier(node)
        {
            Debug.enter("Qualifier",node)

            if( node != void 0 && node.Get != void 0 )
            {
                var result = interpGet(node.Get)
            }
            else
            if( node != void 0 && node.Namespace != void 0 )
            {
                var result = node.Namespace
            }
            else
            {
                throw "invalid Qualifier " + node
            }

            Debug.exit("Qualifier",result)
            return result
        }

        /*

        Resolve Get expression

        */

        avm function interpGet(node)
        {
            Debug.enter("Get",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var namespaces = [interpQualifier(node.QualifiedIdentifier.Qualifier)]
                var ident = node.QualifiedIdentifier.Identifier.@name
                var is_qualified = true
            }
            else
            if( node.Identifier != void 0 )
            {
                var namespaces = []
                var opennss = node.Identifier.OpenNamespaces.* // get open namespaces for this identifier
                for each( var ns in opennss )
                {
                    namespaces.push(ns)
                }
                var ident = node.Identifier.@name
                var is_qualified = false
            }
            else
            {
                throw "Get not implemented for " + node.toXMLString()
            }
            const is_strict = true

            // if namespaces has more than one set, then determine
            // which set provides the match.

            if( node.@kind == "lexical" )
            {
                // For each namespace set in namespaces, search for the
                // target property. If it is found, then jump to Get.

                emitter.FindProperty(ident,namespaces,is_qualified,false,is_strict)
            }
            else
            {
                interpExpression(node.*[0])
            }

            // For each namespace set in namespaces, search for the
            // target property. If it is found, then jump to Get.

            emitter.GetProperty(ident,namespaces,is_qualified,false,false)

            Debug.exit("Get",result)
            return result
        }

        ast function interpGet(node)
        {
            Debug.enter("Get",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var qualifier = interpQualifier(node.QualifiedIdentifier.Qualifier)
                var ident = node.QualifiedIdentifier.Identifier.@name
            }
            else
            if( node.Identifier != void 0 )
            {
                var ident = node.Identifier.@name
            }
            else
            {
                throw "Get not implemented for " + node.toXMLString()
            }

            var parent = node
            var result
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var slots = parent.Prologue.Slot.(Name.QualifiedIdentifier.Identifier.@name == ident)

                    if( slots == void 0 )
                    {
                        continue
                    }

                    if( qualifier != void 0 )
                    {
                        var match_slots = matchQualifier(qualifier,slots)
                    }
                    else
                    {
                        var match_slots = matchOpenNamespaces(node,slots)
                    }

                    if( match_slots.length == 0 )
                    {
                        continue
                    }
                    else
                    if( match_slots.length == 1 )
                    {
                        break
                    }
                    else
                    {
                        throw "ambiguous reference to " + ident
                    }
                }
            }
            while( parent.name() != "Program" )

            if( match_slots.length == 1 )
            {
                state.stack.push(match_slots[0].Value.*)
            }
            else
            {
                throw "unresolved reference to " + ident
            }

            Debug.exit("Get",result)
            return result
        }

        /*

        Set

        */

        avm function interpSet(node)
        {
            Debug.enter("Set",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var namespaces = [interpQualifier(node.QualifiedIdentifier.Qualifier)]
                var ident = node.QualifiedIdentifier.Identifier.@name
                var is_qualified = true
            }
            else
            if( node.Identifier != void 0 )
            {
                var opennss = node.Identifier.OpenNamespaces.*  //getOpenNamespaces(node,ident) // get open namespaces for this identifier
                var namespaces = []
                for each( var ns in opennss )
                {
                    namespaces.push(ns)
                }
                var ident = node.Identifier.@name
                var is_qualified = false
            }
            else
            {
                throw "Set not implemented for " + node.toXMLString()
            }

            if( node.@kind == "lexical" )
            {
                emitter.FindProperty(ident,namespaces,is_qualified,false,false)
                interpExpression(node.*[1])        // value
            }
            else
            {
                interpExpression(node.*[0])        // base object
                interpExpression(node.*[2])        // value
            }

            emitter.SetProperty(ident,namespaces,is_qualified,false,false,false)

            Debug.exit("Set",result)
            return result
        }

        ast function interpSet(node)
        {
            Debug.enter("Set",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var qualifier = interpQualifier(node.QualifiedIdentifier.Qualifier)
                var ident = node.QualifiedIdentifier.Identifier.@name
            }
            else
            if( node.Identifier != void 0 )
            {
                var ident = node.Identifier.@name
            }
            else
            {
                throw "Set not implemented for " + node.toXMLString()
            }

            var parent = node
            var result
            var slot_set = []
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var slots = parent.Prologue.Slot.(Name.QualifiedIdentifier.Identifier.@name == ident)

                    if( slots == void 0 )
                    {
                        continue
                    }
                    
                    if( qualifier != void 0 )
                    {
                        var slot_set = matchQualifier(qualifier,slots)
                    }
                    else
                    {
                        var slot_set = matchOpenNamespaces(node,slots)
                    }

                    if( slot_set.length == 0 )
                    {
                        continue
                    }
                    else
                    if( slot_set.length == 1 )
                    {
                        break
                    }
                    else
                    {
                        throw "ambiguous reference to " + ident
                    }
                }
            }
            while( parent.name() != "Program" )

            if( slot_set.length == 1 )
            {
                interpTo(node.To)

                var temp = state.stack.pop()
                slot_set[0].Value.* = temp
            }
            else
            {
                throw "unresolved reference to " + ident
            }

            Debug.exit("Set",result)
            return result
        }

        /*

        Call

        */

        avm function interpCall(node)
        {
            Debug.enter("Call",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var namespaces = [interpQualifier(node.QualifiedIdentifier.Qualifier)]
                var ident = node.QualifiedIdentifier.Identifier.@name
                var is_qualified = true
            }
            else
            if( node.Identifier != void 0 )
            {
                var namespaces = []
                var opennss = node.Identifier.OpenNamespaces.*  
                for each( var ns in opennss )
                {
                    namespaces.push(ns)
                }
                var ident = node.Identifier.@name
                var is_qualified = false
            }
            else
            {
                throw "Set not implemented for " + node.toXMLString()
            }

            if( node.@kind == "lexical" )
            {
                emitter.FindProperty(ident,namespaces,is_qualified,false,true)
            }
            else
            {
                interpExpression(node.*[0])
            }

			emitter.Dup()
            interpArgs(node.Args)
            var count = node.Args.*.length()
            
            if( node.@is_new == "true" )
            {
                emitter.CallProperty(ident,namespaces,count,is_qualified,false,false,false,true)
            }
            else
            {
                emitter.CallProperty(ident,namespaces,count,is_qualified,false,false,false,false)
            }

            Debug.exit("Call",result)
            return result
        }

        ast function interpCall(node)
        {
            Debug.enter("Call",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var qualifier = interpQualifier(node.QualifiedIdentifier.Qualifier)
                var ident = node.QualifiedIdentifier.Identifier.@name
            }
            else
            if( node.Identifier != void 0 )
            {
                var ident = node.Identifier.@name
            }
            else
            {
                throw "Set not implemented for " + node.toXMLString()
            }

            var parent = node
            var result
            var slot_set = []
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var slots = parent.Prologue.Slot.(Name.QualifiedIdentifier.Identifier.@name == ident)

                    if( slots == void 0 )
                    {
                        continue
                    }
                    
                    if( qualifier != void 0 )
                    {
                        var slot_set = matchQualifier(qualifier,slots)
                    }
                    else
                    {
                        var slot_set = matchOpenNamespaces(node,slots)
                    }

                    if( slot_set.length == 0 )
                    {
                        continue
                    }
                    else
                    if( slot_set.length == 1 )
                    {
                        break
                    }
                    else
                    {
                        throw "ambiguous reference to " + ident
                    }
                }
            }
            while( parent.name() != "Program" )

            if( slot_set.length == 1 )
            {
                interpTo(node.To)

                var temp = state.stack.pop()
                slot_set[0].Value.* = temp
            }
            else
            {
                throw "unresolved reference to " + ident
            }

            Debug.exit("Call",result)
            return result
        }

        /*

        Delete

        */

        avm function interpDelete(node)
        {
            Debug.enter("Delete",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var namespaces = [interpQualifier(node.QualifiedIdentifier.Qualifier)]
                var ident = node.QualifiedIdentifier.Identifier.@name
                var is_qualified = true
            }
            else
            if( node.Identifier != void 0 )
            {
                var namespaces = []
                var opennss = node.Identifier.OpenNamespaces.* // get open namespaces for this identifier
                for each( var ns in opennss )
                {
                    namespaces.push(ns)
                }
                var ident = node.Identifier.@name
                var is_qualified = false
            }
            else
            {
                throw "Delete not implemented for " + node.toXMLString()
            }
            const is_strict = true

            if( node.@kind == "lexical" )
            {
                emitter.FindProperty(ident,namespaces,is_qualified,false,is_strict)
            }
            else
            {
                interpExpression(node.*[0])
            }

            emitter.DeleteProperty(ident,namespaces,is_qualified,false,false)

            Debug.exit("Delete",result)
            return result
        }

        ast function interpDelete(node)
        {
            Debug.enter("Delete",node)

            var result

            Debug.exit("Delete",result)
            return result
        }

        /*

        This

        */

        avm function interpThis(node)
        {
            Debug.enter("This",node)

            emitter.LoadThis()

            Debug.exit("This",result)
            return result
        }

        ast function interpThis(node)
        {
            Debug.enter("This",node)

            // TBD

            Debug.exit("This",result)
            return result
        }

        /*

        Number literal

        */

        avm function interpLiteralNumber(node)
        {
            Debug.enter("LiteralNumber",node)

            //emitter.PushNumber(node.@value)
            emitter.PushInt(node.@value)   // todo: implement PushNumber

            Debug.exit("LiteralNumber",result)
            return result
        }

        ast function interpLiteralNumber(node)
        {
            Debug.enter("LiteralNumber",node)

            state.stack.push(node)

            Debug.exit("LiteralNumber",result)
            return result
        }

        /*
        
        String literal

        */

        avm function interpLiteralString(node)
        {
            Debug.enter("LiteralString",node)

            emitter.PushString(node.@value)
            var result = node

            Debug.exit("LiteralString",result)
            return result
        }

        ast function interpLiteralString(node)
        {
            Debug.enter("LiteralString",node)

            state.stack.push(node)
            var result = node

            Debug.exit("LiteralString",result)
            return result
        }

        /*

        Undefined literal

        */

        avm function interpLiteralUndefined(node)
        {
            Debug.enter("LiteralUndefined",node)

            emitter.PushUndefined()
            var result = node

            Debug.exit("LiteralUndefined",result)
            return result
        }

        ast function interpLiteralUndefined(node)
        {
            Debug.enter("LiteralUndefined",node)

            state.stack.push(node)
            var result = node

            Debug.exit("LiteralUndefined",result)
            return result
        }

        /*

        To

        */

        shared function interpTo(node)
        {
            Debug.enter("To",node)

            interpExpression(node.*[0])
            var result = node

            Debug.exit("To",result)
            return result
        }

        /*

        Args

        */

        shared function interpArgs(node)
        {
            Debug.enter("Args",node)

            for each( var item in node.To )
            {
                interpTo(item)  // for now args are to expressions
            }
            var result = node

            Debug.exit("Args",result)
            return result
        }

        /*
        
        Expression

        */

        shared function interpExpression(node,needs_result:Boolean=true)
        {
            Debug.enter("Expression",node)

            var name = node.localName()
            var result
            switch(name) 
            {
                case "This":
                    if( needs_result ) result = interpThis(node)
                    break
                case "LiteralNumber":
                    if( needs_result ) result = interpLiteralNumber(node)
                    break
                case "LiteralString":
                    if( needs_result ) result = interpLiteralString(node)
                    break
                case "LiteralUndefined":
                    if( needs_result ) result = interpLiteralUndefined(node)
                    break
                case "Get":
                    result = interpGet(node)
                    if( !needs_result ) emitter.Pop()
                    break
                case "Set":
                    result = interpSet(node)
                    if( needs_result ) throw "need to put the result on the stack"
                    break
                case "Call":
                    result = interpCall(node)
                    if( !needs_result ) emitter.Pop()
                    break
                case "Delete":
                    result = interpDelete(node)
                    if( !needs_result ) emitter.Pop()
                    break
                case "To":
                    result = interpTo(node)
                    if( !needs_result ) emitter.Pop()
                    break
                case "Function":
                    result = interpFunction(node)
                    if( !needs_result ) emitter.Pop()
                    break
                default:
                    throw "Expression not implemented for " + node.toXMLString()
            }

            var result = node

            Debug.exit("Expression",result)
            return result
        }

        // template
        function interp(node)
        {
            Debug.enter("",node)

            var result

            Debug.exit("",result)
            return result
        }

        function dumpState(state)
        {
            print('begin state - - - - - - - -')
            
            print('begin stack, size='+state.stack.length)
            for each( var item in state.stack )
            {
               print(' ',item.toXMLString())
            }
            print('end stack')
            print('end state - - - - - - - - -')
        }


    }
}

