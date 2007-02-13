/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



package es4 
{
    use namespace release

    // helper functions

        /*

        Resolve a Get node. Only Get early early can be resolved during
        the definition phase.

        Get
            GetLex early early    ; x, p.q.x, q::x where q is known
            GetObj early early    ; A.x where A is known

        Steps:
            1/match name
            2/match qualifiers
        
        */

        function matchOpenNamespaces(node,slots)
        {
            Debug.enter("matchOpenNamespaces",node.toXMLString(),slots)

            var result = []
            var parent = node
            var ident  = slots[0].Name.QualifiedIdentifier.Identifier.@name
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var opennss = parent.Prologue.OpenNamespaces.(@ident.toString() == "*")
                    if( opennss == void 0 )
                    {
                        continue
                    }
                    else
                    {
                        for each( var ns in opennss.* )
                        {
                            for each( var slot in slots )
                            {
                                if( slot.Name.QualifiedIdentifier.Qualifier.Namespace == ns )
                                {
                                    result.push(slot)
                                }
                            }
                        }
                    }

                    if( result.length == 0 )
                    {
                        continue
                    }
                    else
                    if( result.length == 1 )
                    {
                        break
                    }
                    else
                    {
                        break
                    }
                }
            }
            while( parent.name() != "Program" )

            Debug.exit("matchOpenNamespaces",result)
            return result
        }

        function getOpenNamespaces(node,ident:String)
        {
            Debug.enter("getOpenNamespaces",node.toXMLString(),ident)

            var result = []
            var parent = node
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var opennss = parent.Prologue.OpenNamespaces.(@ident.toString() == "*" || @ident.toString() == ident)
                    if( opennss == void 0 )
                    {
                        continue
                    }
                    else
                    {
                        for each( var ns in opennss.* )
                        {
                            result.push(ns)
                        }
                    }
                }
            }
            while( parent.name() != "Program" )

            Debug.exit("getOpenNamespaces",result)
            return result
        }

        /*

        Match qualifier to slots and return the set of matches.

        For each slot in slots,
            Add slot to matches if the slot name qualifier
                    matches the given qualifier
        Return matches

        */

        function matchQualifier(qualifier,slots)
        {
            Debug.enter("matchQualifier",qualifier.toXMLString(),slots)

            var result = []
            for each( var slot in slots )
            {
                if( slot.Name.QualifiedIdentifier.Qualifier.Namespace == qualifier )
                {
                    result.push(slot)
                }
            }

            Debug.exit("matchQualifier",result)
            return result
        }

        var ticket_number : uint = 0
        function get ticket () : uint 
        {
            return ticket_number++
        }

        function getUniqueName(str=void 0)
        {
            if(str != void 0)
            {
                var name = str+'$'+ticket
            }
            else
            {
                var name = "anonymous$"+ticket
            }
            return name
        }



    /*

    Construct slot tables for all environment frames: global, function, class, instance,
    with, let, and block constructs.

    A slot looks like this: <slot>{name}{type}{value}</slot>

    Evaluate const, let const, namespace, type, class and interface definitions that
    occur at the top level of a program, package or class body (this includes static 
    const but not instance const)

    */

    class Definer
    {
        function Definer()
        {
            ticket_number = 0  // reset 
        }

        function addNamespaceToOpenNamespaces(prologue,ident,namespace)
        {
            Debug.enter("addNamespaceToOpenNamespaces",prologue,ident,namespace)

            var on = prologue.OpenNamespaces.(@ident == ident)
            if( on.length() == 0 )
            {
                on = <OpenNamespaces ident={ident}/>
                if( prologue.*.length() == 0 )  // e4x: insertChildBefore should do the right thing 
                                                // if the first argument is undefined so we don't 
                                                // have to check for that special case
                {
                    prologue.* = on
                }
                else
                {
                    prologue.insertChildBefore(prologue.*[0],on)
                }
                prologue.*[0].* += namespace
            }
            else
            for each( var opennss in prologue.OpenNamespaces )
            {
                if( opennss.@ident == ident )
                {
                    opennss.* += namespace
                }
            }
            Debug.exit("addNamespaceToOpenNamespaces",prologue)
        }

        function defineImport(node)
        {
            Debug.enter("defineImport",node.toXMLString())

            var prologue = node.parent()  // Prologue
            var ident = node.@def
            addNamespaceToOpenNamespaces(prologue,ident,<Namespace kind="public" name={node.@pkg}/>)

            Debug.exit("defineImport",node)
            return node
        }

        function defineUseNamespace(node)
        {
            Debug.enter("defineUseNamespace",node)

            var block = node.parent()  // Block
            var prologue = block.parent().Prologue
            var ns = defineGet(node.Get)
            if( ns == void 0 )
            {
                throw "unknown namespace in use namespace pragma"
            }
/*
            var on = prologue.OpenNamespaces.(@ident.toString()=="*")
            if( on.length() == 0 )
            {
                on = <OpenNamespaces ident="*"/>
                if( prologue.*.length() == 0 )  // e4x: insertChildBefore should do the right thing 
                                                // if the first argument is undefined so we don't 
                                                // have to check for that special case
                {
                    prologue.* = on
                }
                else
                {
                    prologue.insertChildBefore(prologue.*[0],on)
                }
            }

            prologue.*[0].* += ns
*/
            addNamespaceToOpenNamespaces(prologue,"*",ns)

            Debug.exit("defineUseNamespace",node)
            return node
        }

        function defineProgram(node)
        {
            Debug.enter("defineProgram",node)
            
            var hoisted = node.Prologue
            definePrologue(node.Prologue,null)
            defineBlock(node.Block,hoisted)

            Debug.exit("defineProgram",node)
            return node
        }

        function hasDefaultConstructor(name,slots)
        {
            var has_ctor = false
            for each ( s in slots )
            {
                var slot_name = s.Name.QualifiedIdentifier.Identifier.@name
                if( name==slot_name )
                {
                    has_ctor = true
                }
            }

            return has_ctor

        }

        function defineClass(node)
        {
            Debug.enter("defineClass",node)
            
            // Create a slot for the 'construct' method. Use the instance slots from
            // the class Prologue as its slots

            var name = node.ClassName.Identifier.@name

            var static_slots = node.Prologue.Static.*
            delete node.Prologue.Static
            var instance_slots = node.Prologue.Instance.*
            delete node.Prologue.Instance

            if( !hasDefaultConstructor(name,instance_slots) )
            {
                instance_slots +=
                <Slot kind="function">
                    <Name>
                        <QualifiedIdentifier>
                            <Qualifier>
                                <Namespace kind="internal" name="global"/>
                            </Qualifier>
                            <Identifier name={name}/>
                        </QualifiedIdentifier>
                    </Name>
                    <Init>
                        <Function name={name}>
                            <Signature/>
                            <Prologue></Prologue>
                            <Block>
                            </Block>
                        </Function>
                    </Init>
                </Slot>
            }

            instance_slots +=
                <Slot kind="type">
                    <Name>
                        <QualifiedIdentifier>
                            <Qualifier>
                                <Namespace kind="internal" name="global"/>
                            </Qualifier>
                            <Identifier name="type"/>
                        </QualifiedIdentifier>
                    </Name>
                    <Init>
                        <ClassType>
                            <BaseClass/>
                            <FieldType><QualifiedIdentifier/><Type/></FieldType>
                            <FieldType><QualifiedIdentifier/><Type/></FieldType>
                        </ClassType>
                    </Init>
                </Slot>

            var iinit_slot = 
                <Slot kind="function" static="true">
                    <Name>
                        <QualifiedIdentifier>
                            <Qualifier>
                                <Namespace kind="internal" name="global"/>
                            </Qualifier>
                            <Identifier name="construct"/>
                        </QualifiedIdentifier>
                    </Name>
                    <Init>
                        <Function name={name+"$construct"} factory="true">
                            <Signature/>
                            <Prologue>{instance_slots}</Prologue>
                            <Block>
                                <ExpressionStatement>
                                <Call kind="dot">
                                    <This/>
                                    <Identifier name={name}/>
                                </Call>
                                </ExpressionStatement>
                            </Block>
                        </Function>
                    </Init>
                </Slot>

            node.Prologue.* += iinit_slot
            node.Prologue.* += static_slots
            definePrologue(node.Prologue,null)
            defineBlock(node.Block,node.Prologue)

            var result = node

            Debug.exit("defineClass",result)
            return result
        }

        function defineInterface(node)
        {
            Debug.enter("defineInterface",node)
            
            definePrologue(node.Prologue,null)
            var result = node

            Debug.exit("defineInterface",result)
            return result
        }

        /*

		Function

        */

		function getSlot(prologue,slot_name)
		{
			return prologue.*.(Name.QualifiedIdentifier.Identifier.@name == slot_name)	
		}

        function defineFunction(node)
        {
            Debug.enter("defineFunction",node)

            definePrologue(node.Prologue,null)
            defineBlock(node.Block,node.Prologue)

			var slot = getSlot(node.Prologue,"arguments")
			if( slot.@is_param == "true" )
			{
	            var has_user_arguments = true
			}
			else
			{
	            var has_user_arguments = false
			}

            if( !has_user_arguments )
            {
                node.Prologue.* +=

                    <Slot kind="var" is_param="true">
                      <Name>
                        <QualifiedIdentifier>
                          <Qualifier>
                            <Namespace kind="internal" name="global"/>
                          </Qualifier>
                          <Identifier name="arguments"/>
                        </QualifiedIdentifier>
                      </Name>
                      <Type>
                        <Identifier name="*">
                          <OpenNamespaces>
                            <Namespace kind="public" name=""/>
                            <Namespace kind="internal" name="global"/>
                          </OpenNamespaces>
                        </Identifier>
                      </Type>
                    </Slot>

                node.@has_arguments = true
            }

            var result = node

            Debug.exit("defineFunction",result)
            return result
        }

        function definePrologue(node,hoisted)
        {
            Debug.enter("definePrologue",node)

            for each( var n in node.* )
            {
                if( n.name() == "Import" )
                {
                    defineImport(n)
                }
                else
                if( n.name() == "UseNamespace" )
                {
                    defineUseNamespace(n)
                }
                else
                if( n.name() == "Slot" )
                {
                    defineSlot(n,hoisted)
                }
                else
                if( n.name() == "OpenNamespaces" )
                {
                    // do nothing
                }
                else
                {
                    throw "invalid node in Prologue: "+n.name()
                }
            }

            delete node.Import
            delete node.UseNamespace

            Debug.exit("definePrologue",node,hoisted)
            return node
        }

        function defineBlock(node,hoisted)
        {
            Debug.enter("defineBlock",node,hoisted)
            
            for each( var n in node.* )
            {                
                var name = n.name()

                if( name == "BlockStatement" )
                {
                    defineBlockStatement(n,hoisted)
                }
                if( name == "LetStatement" )
                {
                    defineLetStatement(n,hoisted)
                }
                else
                if( name == "ExpressionStatement" )
                {
                    defineExpressionStatement(n,hoisted)
                }
                else
                if( name == "Return" )
                {
                    defineReturnStatement(n,hoisted)
                }
            }

            Debug.exit("defineBlock",node)
            return node
        }

        function defineBlockStatement(node,hoisted)
        {
            Debug.enter("defineBlockStatement",node,hoisted)

            definePrologue(node.Prologue,hoisted)
            defineBlock(node.Block,hoisted)

            Debug.exit("defineBlockStatement",node)
        }

        function defineLetStatement(node,hoisted)
        {
            Debug.enter("defineLetStatement",node)

            definePrologue(node.Prologue,hoisted)
            if( node.BlockStatement != void 0 )
            {
                defineBlockStatement(node.BlockStatement,hoisted)
            }
            else
            {
                throw "not implemented"
            }

            Debug.exit("defineLetStatement",node)
        }

        function defineExpressionStatement(node,hoisted)
        {
            Debug.enter("ExpressionStatement",node)

            var result = defineExpression(node.*)

            Debug.exit("ExpressionStatement",result)
            return result
        }

        function defineReturnStatement(node,hoisted)
        {
            Debug.enter("ReturnStatement",node)

            var result = defineExpression(node.*)

            Debug.exit("ReturnStatement",result)
            return result
        }

        function defineExpression(node)
        {
            Debug.enter("Expression",node)

            if( node.Function != void 0 )
            {
//                defineFunctionExpression(node.Function,hoisted)
            }
            else
            if( node.LetExpression != void 0 )
            {
//                defineLetExpression(node.LetExpression,hoisted)
            }
            else
            if( node.name() == "Get" )
            {
                defineGet(node)
            }
            else
            if( node.name() == "Set" )
            {
                defineSet(node)
            }
            else
            if( node.name() == "Call" )
            {
                defineCall(node)
            }
            else
            if( node.name() == "Delete" )
            {
                defineGet(node)   // define Delete like we do Get
            }
            else
            {
                // ignore
            }

            Debug.exit("Expression",node)
        }

        /*

        Deal with a slot

        This includes evaluating expressions that must have known values during
        the definition phase, hoisting slots that belong to the enclosing 
        variable object (all but let slots), moving function initialisers into
        the block associated with the current prologue, checking for duplicate
        definitions and deleting compatible ones and throwing an error for 
        incompatible ones

        */

        function defineSlot(slot,hoisted)
        {
            Debug.enter("defineSlot",slot)

            // Resolve namespace attributes

            var qualifier = defineQualifier(slot.Name.QualifiedIdentifier.Qualifier)
            if( qualifier == void 0 )
            {
                throw "The value of namespace attribute "+slot.Name.QualifiedIdentifier.Qualifier.Get.Identifier.@name+" is unknown"
            }
            else
            if( qualifier.name() == "Namespace" )
            {
                slot.Name.QualifiedIdentifier.Qualifier.* = qualifier    // replace old qualifier
            }
            else
            {
                throw "invalid namespace attribute " + qualifier
            }

            // Resolve the type

            if( slot.Type != void 0 )
            {
                var type = defineGet(slot.Type)  
                            // defineGet works on any value with an Identifier as a child
print('type',type)
            }

            // Resolve the initialiser

            if( slot.Init != void 0 )
            {
                if( slot.Init.Class != void 0 )
                {
                    slot.Value.* = defineClass(slot.Init.Class)
                }
                else
                if( slot.Init.Interface != void 0 )
                {
                    slot.Value.* = <Namespace kind="interface" name={getUniqueName(slot.Name.QualifiedIdentifier.Identifier.@name)}/>
                    slot.Value.* += defineInterface(slot.Init.Interface)
                }
                else
                if( slot.Init.Function != void 0 )
                {
                    slot.Value.* = defineFunction(slot.Init.Function)
                }
                else
                if( slot.@kind == "type" )
                {
                    slot.Value.* = slot.Init.*
                }
                else
                if( slot.@kind == "var" )
                {                    
                    defineExpression(slot.Init.*)
                    slot.Value.* = slot.Init.* 
                }
                else
                if( slot.@kind == "namespace" )  // todo: move into function
                {
                    if( slot.Init.LiteralString != void 0 )
                    {
                        slot.Value.* = <Namespace kind="user" name={slot.Init.To.LiteralString.@value}/>
                    }
                    else
                    if( slot.Init.UniqueNamespaceName != void 0 )
                    {
                        slot.Value.* = <Namespace kind="user" name={getUniqueName(slot.Name.QualifiedIdentifier.Identifier.@name)}/>
                    }
                    else
                    if( slot.Init.Get != void 0 )
                    {
                        var value = defineGet(slot.Init.Get)
                        if( value != null )
                        {
                            slot.Value.* = value
                        }
                        else
                        {
                            throw "unknown namespace name in namespace definition"
                        }
                    }
                    else
                    {
                        throw "unknown namespace name in namespace definition"
                    }
                }
                if( slot.Value != void 0 )
                {
                    delete slot.Init   // if we have a value then we can nuke the initialiser
                }
            }

            var prologue

            // Hoist, if necessary. If hoisted == null, then we are processing a slot
            // in a immediately nested variable object

            if( hoisted != null && slot.@let != "true" )
            {
                if( slot.@kind=="function" && slot.@method != "true" )
                {
                    slot.@kind = "var"  // its really a var
                    var stmt = <ExpressionStatement><Set kind="lexical">{slot.Name.*}{slot.Value.*}</Set></ExpressionStatement>
                    var block = hoisted.parent().Block
                    if( block.*.length() == 0 )
                    {
                        block.* += stmt
                    }
                    else
                    {
                        block.insertChildBefore(block.*[0],stmt)
                    }
                    delete slot.Value
                }
                hoisted.* += slot   // This line orphans 'slot' and its peers if slot.parent() == hoisted
                slot.setLocalName("hoisted")
                prologue = hoisted
                delete slot.parent().hoisted
            }
            else
            {
                prologue = slot.parent()
            }

            // Check for duplicates

            var ident = slot.Name.QualifiedIdentifier.Identifier.@name
            var qualifier = slot.Name.QualifiedIdentifier.Qualifier.Namespace
            var type = slot.Type
            var slots = prologue.Slot.(Name.QualifiedIdentifier.Identifier.@name == ident)
            if( slots != void 0 )
            {
                slots = matchQualifier(qualifier,slots)
                if( slots.length == 1 )
                {
                    // no conflict
                }
                else
                {
                    for each ( s in slots )
                    {
                        if( !matchType(s.Type,type) )
                        {
                            throw "incompatible override of slot " + ident
                        }
                        else
                        if( s !== slots[0] )  // skip the first slot to avoid false positive
                        {
                            s.setLocalName("duplicate")    
                        }
                    }
                    delete prologue.duplicate
                }
            }

            Debug.exit("defineSlot",slot)
            return slot
        }

        function matchType(t1,t2)  // todo: needs to be more robust
        {
            Debug.enter("matchType",t1.toXMLString(),t2.toXMLString())

            var result = false
            if( t1.Identifier != void 0 || t2.Identifier != void 0 )
            {
                result = t1.Identifier.@name == t2.Identifier.@name
            }
            else
            if( t1 == t2 )
            {
                result = true
            }

            Debug.exit("matchType",result)
            return result
        }

        function defineQualifier(node)
        {
            Debug.enter("defineQualifier",node)

            if( node != void 0 && node.Get != void 0 )
            {
                var result = defineGet(node.Get)
            }
            else
            if( node != void 0 && node.Namespace != void 0 )
            {
                var result = node.Namespace
            }
            else
            {
                var result = node
            }

            Debug.exit("defineQualifier",result)
            return result
        }

        /*

        Resolve Get expression

        It is an error to for a Get to resolve to zero or more
        than one slot.

        */

        function defineGet(node)
        {
            Debug.enter("defineGet",node)

            if( node.QualifiedIdentifier != void 0 )
            {
                var qualifier = defineQualifier(node.QualifiedIdentifier.Qualifier)
                if( qualifier != void 0 && qualifier.name() == "Namespace" ) // convert to a single multiname
                {
                    node.QualifiedIdentifier = node.QualifiedIdentifier.Identifier
                    node.Identifier.OpenNamespaces.* = qualifier   // replace expression with ct const value
                }
                var ident = node.QualifiedIdentifier.Identifier.@name
            }
            else
            if( node.Identifier != void 0 )
            {
                var ident = node.Identifier.@name
                var namespaces = getOpenNamespaces(node,ident) // get open namespaces for this identifier
                for each ( var ns in namespaces )
                {
                    node.Identifier.OpenNamespaces.* += ns
                }
            }
            else
            {
                throw "defineGet not implemented for " + node.toXMLString()
            }

            var parent = node
            var result

            if( node.@kind.toString() == "dot" )
            {
                defineExpression(node.*[0])  // don't worry about the result at this time
            }
            else
            do
            {
                var parent = parent.parent()
                if( parent.Prologue != void 0 )
                {
                    var slots = parent.Prologue.Slot.(Name.QualifiedIdentifier.Identifier.@name == ident)
//print("prologue",parent.Prologue)
//print("found",parent.name(),ident,slots.length())
                    if( slots == void 0 )
                    {
                        continue
                    }

                    if( qualifier != void 0 )
                    {
                        slots = matchQualifier(qualifier,slots)
                    }
                    else
                    {
                        slots = matchOpenNamespaces(node,slots)
                    }

                    if( slots.length == 0 )
                    {
                        continue
                    }
                    else
                    if( slots.length == 1 )
                    {
                        result = slots[0].Value.*
                        break
                    }
                    else
                    {
                        throw "ambiguous reference to " + ident
                    }
                }
            }
            while( parent.name() != "Program" )

            Debug.exit("defineGet",result)
            return result
        }

        function defineSet(node)
        {
            Debug.enter("defineSet",node.toXMLString())

            var result = defineGet(node)
            defineTo(node.To)

            Debug.exit("defineSet")
            return result
        }

        function defineCall(node)
        {
            Debug.enter("defineCall",node.toXMLString())

            var result = defineGet(node)
            defineArgs(node.Args)

            Debug.exit("defineCall")
            return result
        }

        function defineTo(node)
        {
            Debug.enter("To",node)

            defineExpression(node.*[0])
            var result = node

            Debug.exit("To")
            return result
        }

        function defineArgs(node)
        {
            Debug.enter("Args",node)

            for each( var item in node.To )
            {
                defineTo(item)  // for now args are to expressions
            }
            var result = node

            Debug.exit("Args")
            return result
        }

    }
}
