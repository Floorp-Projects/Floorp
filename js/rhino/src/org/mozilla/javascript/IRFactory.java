/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Norris Boyd
 * Igor Bukanov
 * Ethan Hugg
 * Terry Lucas
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript;

/**
 * This class allows the creation of nodes, and follows the Factory pattern.
 *
 * @see Node
 * @author Mike McCabe
 * @author Norris Boyd
 */
final class IRFactory
{
    IRFactory(Parser parser)
    {
        this.parser = parser;
    }

    ScriptOrFnNode createScript()
    {
        return new ScriptOrFnNode(Token.SCRIPT);
    }

    /**
     * Script (for associating file/url names with toplevel scripts.)
     */
    void initScript(ScriptOrFnNode scriptNode, Node body)
    {
        Node children = body.getFirstChild();
        if (children != null) { scriptNode.addChildrenToBack(children); }
    }

    /**
     * Leaf
     */
    Node createLeaf(int nodeType)
    {
        return new Node(nodeType);
    }

    Node createLeaf(int nodeType, int nodeOp)
    {
        return new Node(nodeType, nodeOp);
    }

    /**
     * Statement leaf nodes.
     */

    Node createSwitch(Node expr, int lineno)
    {
        return new Node.Jump(Token.SWITCH, expr, lineno);
    }

    void addSwitchCase(Node switchNode, Node expression, Node statements)
    {
        if (switchNode.getType() != Token.SWITCH) throw Kit.codeBug();
        Node caseNode = new Node(Token.CASE, expression, statements);
        switchNode.addChildToBack(caseNode);
    }

    void addSwitchDefault(Node switchNode, Node statements)
    {
        if (switchNode.getType() != Token.SWITCH) throw Kit.codeBug();
        Node defaultNode = new Node(Token.DEFAULT, statements);
        switchNode.addChildToBack(defaultNode);
    }

    Node createVariables(int lineno)
    {
        return new Node(Token.VAR, lineno);
    }

    Node createExprStatement(Node expr, int lineno)
    {
        int type;
        if (parser.insideFunction()) {
            type = Token.EXPR_VOID;
        } else {
            type = Token.EXPR_RESULT;
        }
        return new Node(type, expr, lineno);
    }

    Node createExprStatementNoReturn(Node expr, int lineno)
    {
        return new Node(Token.EXPR_VOID, expr, lineno);
    }

    Node createDefaultNamespace(Node expr, int lineno)
    {
        // default xml namespace requires activation
        setRequiresActivation();
        Node n = createUnary(Token.DEFAULTNAMESPACE, expr);
        Node result = createExprStatement(n, lineno);
        return result;
    }

    /**
     * Name
     */
    Node createName(String name)
    {
        checkActivationName(name, Token.NAME);
        return Node.newString(Token.NAME, name);
    }

    /**
     * String (for literals)
     */
    Node createString(String string)
    {
        return Node.newString(string);
    }

    /**
     * Number (for literals)
     */
    Node createNumber(double number)
    {
        return Node.newNumber(number);
    }

    /**
     *  PropertySelector :: PropertySelector
     */
    Node createQualifiedName(String namespace, String name)
    {
        Node namespaceNode = createString(namespace);
        Node nameNode = createString(name);
        return new Node(Token.COLONCOLON, namespaceNode, nameNode);
    }

    /**
     *  PropertySelector :: [Expression]
     */
    Node createQualifiedExpr(String namespace, Node expr)
    {
        Node namespaceNode = createString(namespace);
        return new Node(Token.COLONCOLON, namespaceNode, expr);
    }

    /**
     *  @PropertySelector or @QualifiedIdentifier
     */
    Node createAttributeName(Node nameNode)
    {
        int type = nameNode.getType();
        if (type == Token.NAME) {
            nameNode.setType(Token.STRING);
        } else {
            // If not name, then it should come from createQualifiedExpr
            if (type != Token.COLONCOLON)
                throw new IllegalArgumentException(String.valueOf(type));
        }
        return new Node(Token.TOATTRNAME, nameNode);
    }

    /**
     *  @::[Expression]
     */
    Node createAttributeExpr(Node expr)
    {
        return new Node(Token.TOATTRNAME, expr);
    }

    Node createXMLPrimary(Node xmlName)
    {
        Node xmlRef = new Node(Token.XML_REF, xmlName);
        return new Node(Token.GET_REF, xmlRef);
    }

    /**
     * Catch clause of try/catch/finally
     * @param varName the name of the variable to bind to the exception
     * @param catchCond the condition under which to catch the exception.
     *                  May be null if no condition is given.
     * @param stmts the statements in the catch clause
     * @param lineno the starting line number of the catch clause
     */
    Node createCatch(String varName, Node catchCond, Node stmts, int lineno)
    {
        if (catchCond == null) {
            catchCond = new Node(Token.EMPTY);
        }
        return new Node(Token.CATCH, createName(varName),
                        catchCond, stmts, lineno);
    }

    /**
     * Throw
     */
    Node createThrow(Node expr, int lineno)
    {
        return new Node(Token.THROW, expr, lineno);
    }

    /**
     * Return
     */
    Node createReturn(Node expr, int lineno)
    {
        return expr == null
            ? new Node(Token.RETURN, lineno)
            : new Node(Token.RETURN, expr, lineno);
    }

    /**
     * Label
     */
    Node createLabel(String label, int lineno)
    {
        Node.Jump n = new Node.Jump(Token.LABEL, lineno);
        n.setLabel(label);
        return n;
    }

    /**
     * Break (possibly labeled)
     */
    Node createBreak(String label, int lineno)
    {
        Node.Jump n = new Node.Jump(Token.BREAK, lineno);
        if (label != null) {
            n.setLabel(label);
        }
        return n;
    }

    /**
     * Continue (possibly labeled)
     */
    Node createContinue(String label, int lineno)
    {
        Node.Jump n = new Node.Jump(Token.CONTINUE, lineno);
        if (label != null) {
            n.setLabel(label);
        }
        return n;
    }

    /**
     * Statement block
     * Creates the empty statement block
     * Must make subsequent calls to add statements to the node
     */
    Node createBlock(int lineno)
    {
        return new Node(Token.BLOCK, lineno);
    }

    FunctionNode createFunction(String name)
    {
        return new FunctionNode(name);
    }

    Node initFunction(FunctionNode fnNode, int functionIndex,
                      Node statements, int functionType)
    {
        fnNode.setFunctionType(functionType);
        fnNode.addChildToBack(statements);

        int functionCount = fnNode.getFunctionCount();
        if (functionCount != 0) {
            // Functions containing other functions require activation objects
            fnNode.setRequiresActivation();
            for (int i = 0; i != functionCount; ++i) {
                FunctionNode fn = fnNode.getFunctionNode(i);
                // nested function expression statements overrides var
                if (fn.getFunctionType()
                        == FunctionNode.FUNCTION_EXPRESSION_STATEMENT)
                {
                    String name = fn.getFunctionName();
                    if (name != null && name.length() != 0) {
                        fnNode.removeParamOrVar(name);
                    }
                }
            }
        }

        if (functionType == FunctionNode.FUNCTION_EXPRESSION) {
            String name = fnNode.getFunctionName();
            if (name != null && name.length() != 0
                && !fnNode.hasParamOrVar(name))
            {
                // A function expression needs to have its name as a
                // variable (if it isn't already allocated as a variable).
                // See ECMA Ch. 13.  We add code to the beginning of the
                // function to initialize a local variable of the
                // function's name to the function value.
                fnNode.addVar(name);
                Node setFn = new Node(Token.EXPR_VOID,
                                new Node(Token.SETVAR, Node.newString(name),
                                         new Node(Token.THISFN)));
                statements.addChildrenToFront(setFn);
            }
        }

        // Add return to end if needed.
        Node lastStmt = statements.getLastChild();
        if (lastStmt == null || lastStmt.getType() != Token.RETURN) {
            statements.addChildToBack(new Node(Token.RETURN));
        }

        Node result = Node.newString(Token.FUNCTION,
                                     fnNode.getFunctionName());
        result.putIntProp(Node.FUNCTION_PROP, functionIndex);
        return result;
    }

    /**
     * Add a child to the back of the given node.  This function
     * breaks the Factory abstraction, but it removes a requirement
     * from implementors of Node.
     */
    void addChildToBack(Node parent, Node child)
    {
        parent.addChildToBack(child);
    }

    /**
     * While
     */
    Node createWhile(Node cond, Node body, int lineno)
    {
        return createLoop(LOOP_WHILE, body, cond, null, null, lineno);
    }

    /**
     * DoWhile
     */
    Node createDoWhile(Node body, Node cond, int lineno)
    {
        return createLoop(LOOP_DO_WHILE, body, cond, null, null, lineno);
    }

    /**
     * For
     */
    Node createFor(Node init, Node test, Node incr, Node body, int lineno)
    {
        return createLoop(LOOP_FOR, body, test, init, incr, lineno);
    }

    private Node createLoop(int loopType, Node body, Node cond,
                            Node init, Node incr, int lineno)
    {
        Node.Target bodyTarget = new Node.Target();
        Node.Target condTarget = new Node.Target();
        if (loopType == LOOP_FOR && cond.getType() == Token.EMPTY) {
            cond = new Node(Token.TRUE);
        }
        Node.Jump IFEQ = new Node.Jump(Token.IFEQ, cond);
        IFEQ.target = bodyTarget;
        Node.Target breakTarget = new Node.Target();

        Node.Jump result = new Node.Jump(Token.LOOP, lineno);
        result.addChildToBack(bodyTarget);
        result.addChildrenToBack(body);
        if (loopType == LOOP_WHILE || loopType == LOOP_FOR) {
            // propagate lineno to condition
            result.addChildrenToBack(new Node(Token.EMPTY, lineno));
        }
        result.addChildToBack(condTarget);
        result.addChildToBack(IFEQ);
        result.addChildToBack(breakTarget);

        result.target = breakTarget;
        Node.Target continueTarget = condTarget;

        if (loopType == LOOP_WHILE || loopType == LOOP_FOR) {
            // Just add a GOTO to the condition in the do..while
            Node.Jump GOTO = new Node.Jump(Token.GOTO);
            GOTO.target = condTarget;
            result.addChildToFront(GOTO);

            if (loopType == LOOP_FOR) {
                if (init.getType() != Token.EMPTY) {
                    if (init.getType() != Token.VAR) {
                        init = new Node(Token.EXPR_VOID, init);
                    }
                    result.addChildToFront(init);
                }
                Node.Target incrTarget = new Node.Target();
                result.addChildAfter(incrTarget, body);
                if (incr.getType() != Token.EMPTY) {
                    incr = new Node(Token.EXPR_VOID, incr);
                    result.addChildAfter(incr, incrTarget);
                }
                continueTarget = incrTarget;
            }
        }

        result.setContinue(continueTarget);

        return result;
    }

    /**
     * For .. In
     *
     */
    Node createForIn(Node lhs, Node obj, Node body, boolean isForEach,
                     int lineno)
    {
        String name;
        int type = lhs.getType();

        Node lvalue = lhs;
        switch (type) {

          case Token.NAME:
          case Token.GETPROP:
          case Token.GETELEM:
          case Token.GET_REF:
            break;

          case Token.VAR:
            /*
             * check that there was only one variable given.
             * we can't do this in the parser, because then the
             * parser would have to know something about the
             * 'init' node of the for-in loop.
             */
            Node lastChild = lhs.getLastChild();
            if (lhs.getFirstChild() != lastChild) {
                parser.reportError("msg.mult.index");
            }
            lvalue = Node.newString(Token.NAME, lastChild.getString());
            break;

          default:
            parser.reportError("msg.bad.for.in.lhs");
            return obj;
        }

        Node localBlock = new Node(Token.LOCAL_BLOCK);

        int initType = (isForEach) ? Token.ENUM_INIT_VALUES
                                   : Token.ENUM_INIT_KEYS;
        Node init = new Node(initType, obj);
        init.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
        Node cond = new Node(Token.ENUM_NEXT);
        cond.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
        Node id = new Node(Token.ENUM_ID);
        id.putProp(Node.LOCAL_BLOCK_PROP, localBlock);

        Node newBody = new Node(Token.BLOCK);
        Node assign = createAssignment(lvalue, id);
        newBody.addChildToBack(new Node(Token.EXPR_VOID, assign));
        newBody.addChildToBack(body);

        Node loop = createWhile(cond, newBody, lineno);
        loop.addChildToFront(init);
        if (type == Token.VAR)
            loop.addChildToFront(lhs);
        localBlock.addChildToBack(loop);

        return localBlock;
    }

    /**
     * Try/Catch/Finally
     *
     * The IRFactory tries to express as much as possible in the tree;
     * the responsibilities remaining for Codegen are to add the Java
     * handlers: (Either (but not both) of TARGET and FINALLY might not
     * be defined)

     * - a catch handler for javascript exceptions that unwraps the
     * exception onto the stack and GOTOes to the catch target

     * - a finally handler

     * ... and a goto to GOTO around these handlers.
     */
    Node createTryCatchFinally(Node tryBlock, Node catchBlocks,
                               Node finallyBlock, int lineno)
    {
        boolean hasFinally = (finallyBlock != null)
                             && (finallyBlock.getType() != Token.BLOCK
                                 || finallyBlock.hasChildren());

        // short circuit
        if (tryBlock.getType() == Token.BLOCK && !tryBlock.hasChildren()
            && !hasFinally)
        {
            return tryBlock;
        }

        boolean hasCatch = catchBlocks.hasChildren();

        // short circuit
        if (!hasFinally && !hasCatch)  {
            // bc finally might be an empty block...
            return tryBlock;
        }


        Node localBlock  = new Node(Token.LOCAL_BLOCK);
        Node.Jump pn = new Node.Jump(Token.TRY, tryBlock, lineno);
        pn.putProp(Node.LOCAL_BLOCK_PROP, localBlock);

        Node.Target finallyTarget = null;
        if (hasFinally) {
            // make a TARGET for the finally that the tcf node knows about
            finallyTarget = new Node.Target();
            pn.setFinally(finallyTarget);

            // add jsr finally to the try block
            Node.Jump jsrFinally = new Node.Jump(Token.JSR);
            jsrFinally.target = finallyTarget;
            pn.addChildToBack(jsrFinally);
        }

        Node.Target endTarget = new Node.Target();
        Node.Jump GOTOToEnd = new Node.Jump(Token.GOTO);
        GOTOToEnd.target = endTarget;
        pn.addChildToBack(GOTOToEnd);

        if (hasCatch) {
            /*
             *
               Given

                try {
                        throw 3;
                } catch (e if e instanceof Object) {
                        print("object");
                } catch (e2) {
                        print(e2);
                }

               rewrite as

                try {
                        throw 3;
                } catch (x) {
                        with (newCatchScope(e, x)) {
                                if (e instanceof Object) {
                                        print("object");
                                }
                        }
                        with (newCatchScope(e2, x)) {
                                if (true) {
                                        print(e2);
                                }
                        }
                }
            */
            // make a TARGET for the catch that the tcf node knows about
            Node.Target catchTarget = new Node.Target();
            pn.target = catchTarget;
            // mark it
            pn.addChildToBack(catchTarget);

            Node.Target endCatch = new Node.Target();

            // add [jsr finally?] goto end to each catch block
            // expects catchblocks children to be (cond block) pairs.
            Node cb = catchBlocks.getFirstChild();
            boolean hasDefault = false;
            while (cb != null) {
                int catchLineNo = cb.getLineno();

                Node name = cb.getFirstChild();
                Node cond = name.getNext();
                Node catchBlock = cond.getNext();
                cb.removeChild(name);
                cb.removeChild(cond);
                cb.removeChild(catchBlock);

                catchBlock.addChildToBack(new Node(Token.LEAVEWITH));
                Node.Jump GOTOToEndCatch = new Node.Jump(Token.GOTO);
                GOTOToEndCatch.target = endCatch;
                catchBlock.addChildToBack(GOTOToEndCatch);
                Node condStmt;
                if (cond.getType() == Token.EMPTY) {
                    condStmt = catchBlock;
                    hasDefault = true;
                } else {
                    condStmt = createIf(cond, catchBlock, null, catchLineNo);
                }
                // Try..catch produces "with" code in order to limit
                // the scope of the exception object.
                // OPT: We should be able to figure out the correct
                //      scoping at compile-time and avoid the
                //      runtime overhead.
                Node catchScope = Node.newString(Token.CATCH_SCOPE,
                                                 name.getString());
                catchScope.addChildToBack(createUseLocal(localBlock));
                Node withStmt = createWith(catchScope, condStmt, catchLineNo);
                pn.addChildToBack(withStmt);

                // move to next cb
                cb = cb.getNext();
            }
            if (!hasDefault) {
                // Generate code to rethrow if no catch clause was executed
                Node rethrow = new Node(Token.THROW,
                                        createUseLocal(localBlock));
                pn.addChildToBack(rethrow);
            }

            pn.addChildToBack(endCatch);
            // add a JSR finally if needed
            if (hasFinally) {
                Node.Jump jsrFinally = new Node.Jump(Token.JSR);
                jsrFinally.target = finallyTarget;
                pn.addChildToBack(jsrFinally);
                Node.Jump GOTO = new Node.Jump(Token.GOTO);
                GOTO.target = endTarget;
                pn.addChildToBack(GOTO);
            }
        }

        if (hasFinally) {
            pn.addChildToBack(finallyTarget);
            Node fBlock = new Node(Token.FINALLY, finallyBlock);
            fBlock.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
            pn.addChildToBack(fBlock);
        }
        pn.addChildToBack(endTarget);
        localBlock.addChildToBack(pn);
        return localBlock;
    }

    /**
     * Throw, Return, Label, Break and Continue are defined in ASTFactory.
     */

    /**
     * With
     */
    Node createWith(Node obj, Node body, int lineno)
    {
        setRequiresActivation();
        Node result = new Node(Token.BLOCK, lineno);
        result.addChildToBack(new Node(Token.ENTERWITH, obj));
        Node bodyNode = new Node(Token.WITH, body, lineno);
        result.addChildrenToBack(bodyNode);
        result.addChildToBack(new Node(Token.LEAVEWITH));
        return result;
    }

    /**
     * DOTQUERY
     */
    public Node createDotQuery (Node obj, Node body, int lineno)
    {
        setRequiresActivation();
        Node result = new Node(Token.DOTQUERY, obj, body, lineno);
        return result;
    }

    Node createArrayLiteral(ObjArray elems, int skipCount)
    {
        int length = elems.size();
        int[] skipIndexes = null;
        if (skipCount != 0) {
            skipIndexes = new int[skipCount];
        }
        Node array = new Node(Token.ARRAYLIT);
        for (int i = 0, j = 0; i != length; ++i) {
            Node elem = (Node)elems.get(i);
            if (elem != null) {
                array.addChildToBack(elem);
            } else {
                skipIndexes[j] = i;
                ++j;
            }
        }
        if (skipCount != 0) {
            array.putProp(Node.SKIP_INDEXES_PROP, skipIndexes);
        }
        return array;
    }

    /**
     * Object Literals
     * <BR> createObjectLiteral rewrites its argument as object
     * creation plus object property entries, so later compiler
     * stages don't need to know about object literals.
     */
    Node createObjectLiteral(ObjArray elems)
    {
        int size = elems.size() / 2;
        Node object = new Node(Token.OBJECTLIT);
        Object[] properties;
        if (size == 0) {
            properties = ScriptRuntime.emptyArgs;
        } else {
            properties = new Object[size];
            for (int i = 0; i != size; ++i) {
                properties[i] = elems.get(2 * i);
                Node value = (Node)elems.get(2 * i + 1);
                object.addChildToBack(value);
            }
        }
        object.putProp(Node.OBJECT_IDS_PROP, properties);
        return object;
    }

    /**
     * Regular expressions
     */
    Node createRegExp(int regexpIndex)
    {
        Node n = new Node(Token.REGEXP);
        n.putIntProp(Node.REGEXP_PROP, regexpIndex);
        return n;
    }

    /**
     * If statement
     */
    Node createIf(Node cond, Node ifTrue, Node ifFalse, int lineno)
    {
        int condStatus = isAlwaysDefinedBoolean(cond);
        if (condStatus == ALWAYS_TRUE_BOOLEAN) {
            return ifTrue;
        } else if (condStatus == ALWAYS_FALSE_BOOLEAN) {
            if (ifFalse != null) {
                return ifFalse;
            }
            // Replace if (false) xxx by empty block
            return new Node(Token.BLOCK, lineno);
        }

        Node result = new Node(Token.BLOCK, lineno);
        Node.Target ifNotTarget = new Node.Target();
        Node.Jump IFNE = new Node.Jump(Token.IFNE, cond);
        IFNE.target = ifNotTarget;

        result.addChildToBack(IFNE);
        result.addChildrenToBack(ifTrue);

        if (ifFalse != null) {
            Node.Jump GOTOToEnd = new Node.Jump(Token.GOTO);
            Node.Target endTarget = new Node.Target();
            GOTOToEnd.target = endTarget;
            result.addChildToBack(GOTOToEnd);
            result.addChildToBack(ifNotTarget);
            result.addChildrenToBack(ifFalse);
            result.addChildToBack(endTarget);
        } else {
            result.addChildToBack(ifNotTarget);
        }

        return result;
    }

    Node createCondExpr(Node cond, Node ifTrue, Node ifFalse)
    {
        int condStatus = isAlwaysDefinedBoolean(cond);
        if (condStatus == ALWAYS_TRUE_BOOLEAN) {
            return ifTrue;
        } else if (condStatus == ALWAYS_FALSE_BOOLEAN) {
            return ifFalse;
        }
        return new Node(Token.HOOK, cond, ifTrue, ifFalse);
    }

    /**
     * Unary
     */
    Node createUnary(int nodeType, Node child)
    {
        int childType = child.getType();
        switch (nodeType) {
          case Token.DELPROP: {
            Node n;
            if (childType == Token.NAME) {
                // Transform Delete(Name "a")
                //  to Delete(Bind("a"), String("a"))
                child.setType(Token.BINDNAME);
                Node left = child;
                Node right = Node.newString(child.getString());
                n = new Node(nodeType, left, right);
            } else if (childType == Token.GETPROP ||
                       childType == Token.GETELEM)
            {
                Node left = child.getFirstChild();
                Node right = child.getLastChild();
                child.removeChild(left);
                child.removeChild(right);
                n = new Node(nodeType, left, right);
            } else if (childType == Token.GET_REF) {
                Node ref = child.getFirstChild();
                child.removeChild(ref);
                n = new Node(Token.DELPROP, ref);
            } else {
                n = new Node(Token.TRUE);
            }
            return n;
          }
          case Token.TYPEOF:
            if (childType == Token.NAME) {
                child.setType(Token.TYPEOFNAME);
                return child;
            }
            break;
          case Token.BITNOT:
            if (childType == Token.NUMBER) {
                int value = ScriptRuntime.toInt32(child.getDouble());
                child.setDouble(~value);
                return child;
            }
            break;
          case Token.NEG:
            if (childType == Token.NUMBER) {
                child.setDouble(-child.getDouble());
                return child;
            }
            break;
          case Token.NOT: {
            int status = isAlwaysDefinedBoolean(child);
            if (status != 0) {
                int type;
                if (status == ALWAYS_TRUE_BOOLEAN) {
                    type = Token.FALSE;
                } else {
                    type = Token.TRUE;
                }
                if (childType == Token.TRUE || childType == Token.FALSE) {
                    child.setType(type);
                    return child;
                }
                return new Node(type);
            }
            break;
          }
        }
        return new Node(nodeType, child);
    }

    Node createCallOrNew(int nodeType, Node child)
    {
        int type = Node.NON_SPECIALCALL;
        if (child.getType() == Token.NAME) {
            String name = child.getString();
            if (name.equals("eval")) {
                type = Node.SPECIALCALL_EVAL;
            } else if (name.equals("With")) {
                type = Node.SPECIALCALL_WITH;
            }
        } else if (child.getType() == Token.GETPROP) {
            String name = child.getLastChild().getString();
            if (name.equals("eval")) {
                type = Node.SPECIALCALL_EVAL;
            }
        }
        Node node = new Node(nodeType, child);
        if (type != Node.NON_SPECIALCALL) {
            // Calls to these functions require activation objects.
            setRequiresActivation();
            node.putIntProp(Node.SPECIALCALL_PROP, type);
        }
        return node;
    }

    Node createIncDec(int nodeType, boolean post, Node child)
    {
        int childType = child.getType();

        switch (childType) {
          case Token.NAME:
          case Token.GETPROP:
          case Token.GETELEM:
          case Token.GET_REF: {
            Node n = new Node(nodeType, child);
            int incrDecrMask = 0;
            if (nodeType == Token.DEC) {
                incrDecrMask |= Node.DECR_FLAG;
            }
            if (post) {
                incrDecrMask |= Node.POST_FLAG;
            }
            n.putIntProp(Node.INCRDECR_PROP, incrDecrMask);
            return n;
          }
        }
        return createIncDec(nodeType, post, makeReferenceGet(child));
    }

    private Node makeReferenceGet(Node node)
    {
        Node ref;
        if (node.getType() == Token.CALL) {
            node.setType(Token.REF_CALL);
            ref = node;
        } else {
            ref = new Node(Token.GENERIC_REF, node);
        }
        return new Node(Token.GET_REF, ref);
    }

    /**
     * Binary
     */
    Node createBinary(int nodeType, Node left, Node right)
    {
        switch (nodeType) {

          case Token.DOT:
            if (right.getType() == Token.NAME) {
                String id = right.getString();
                if (ScriptRuntime.isSpecialProperty(id)) {
                    Node ref = new Node(Token.SPECIAL_REF, left);
                    ref.putProp(Node.SPECIAL_PROP_PROP, id);
                    return new Node(Token.GET_REF, ref);
                }
                nodeType = Token.GETPROP;
                right.setType(Token.STRING);
                checkActivationName(id, Token.GETPROP);
            } else {
                // corresponds to name.@... or name.namespace::...
                nodeType = Token.GETELEM;
            }
            break;

          case Token.DOTDOT:
            if (right.getType() == Token.NAME) {
                right.setType(Token.STRING);
            }
            right = new Node(Token.DESCENDANTS, right);
            nodeType = Token.GETELEM;
            break;

          case Token.LB:
            // OPT: could optimize to GETPROP iff string can't be a number
            nodeType = Token.GETELEM;
            break;

          case Token.ADD:
            // numerical addition and string concatenation
            if (left.type == Token.STRING) {
                String s2;
                if (right.type == Token.STRING) {
                    s2 = right.getString();
                } else if (right.type == Token.NUMBER) {
                    s2 = ScriptRuntime.numberToString(right.getDouble(), 10);
                } else {
                    break;
                }
                String s1 = left.getString();
                left.setString(s1.concat(s2));
                return left;
            } else if (left.type == Token.NUMBER) {
                if (right.type == Token.NUMBER) {
                    left.setDouble(left.getDouble() + right.getDouble());
                    return left;
                } else if (right.type == Token.STRING) {
                    String s1, s2;
                    s1 = ScriptRuntime.numberToString(left.getDouble(), 10);
                    s2 = right.getString();
                    right.setString(s1.concat(s2));
                    return right;
                }
            }
            // can't do anything if we don't know  both types - since
            // 0 + object is supposed to call toString on the object and do
            // string concantenation rather than addition
            break;

          case Token.SUB:
            // numerical subtraction
            if (left.type == Token.NUMBER) {
                double ld = left.getDouble();
                if (right.type == Token.NUMBER) {
                    //both numbers
                    left.setDouble(ld - right.getDouble());
                    return left;
                } else if (ld == 0.0) {
                    // first 0: 0-x -> -x
                    return new Node(Token.NEG, right);
                }
            } else if (right.type == Token.NUMBER) {
                if (right.getDouble() == 0.0) {
                    //second 0: x - 0 -> +x
                    // can not make simply x because x - 0 must be number
                    return new Node(Token.POS, left);
                }
            }
            break;

          case Token.MUL:
            // numerical multiplication
            if (left.type == Token.NUMBER) {
                double ld = left.getDouble();
                if (right.type == Token.NUMBER) {
                    //both numbers
                    left.setDouble(ld * right.getDouble());
                    return left;
                } else if (ld == 1.0) {
                    // first 1: 1 *  x -> +x
                    return new Node(Token.POS, right);
                }
            } else if (right.type == Token.NUMBER) {
                if (right.getDouble() == 1.0) {
                    //second 1: x * 1 -> +x
                    // can not make simply x because x - 0 must be number
                    return new Node(Token.POS, left);
                }
            }
            // can't do x*0: Infinity * 0 gives NaN, not 0
            break;

          case Token.DIV:
            // number division
            if (right.type == Token.NUMBER) {
                double rd = right.getDouble();
                if (left.type == Token.NUMBER) {
                    // both constants -- just divide, trust Java to handle x/0
                    left.setDouble(left.getDouble() / rd);
                    return left;
               } else if (rd == 1.0) {
                    // second 1: x/1 -> +x
                    // not simply x to force number convertion
                    return new Node(Token.POS, left);
                }
            }
            break;

          case Token.AND: {
            int leftStatus = isAlwaysDefinedBoolean(left);
            if (leftStatus == ALWAYS_FALSE_BOOLEAN) {
                // if the first one is false, replace with FALSE
                return new Node(Token.FALSE);
            } else if (leftStatus == ALWAYS_TRUE_BOOLEAN) {
                // if first is true, set to second
                return right;
            }
            int rightStatus = isAlwaysDefinedBoolean(right);
            if (rightStatus == ALWAYS_FALSE_BOOLEAN) {
                // if the second one is false, replace with FALSE
                if (!hasSideEffects(left)) {
                    return new Node(Token.FALSE);
                }
            } else if (rightStatus == ALWAYS_TRUE_BOOLEAN) {
                // if second is true, set to first
                return left;
            }
            break;
          }

          case Token.OR: {
            int leftStatus = isAlwaysDefinedBoolean(left);
            if (leftStatus == ALWAYS_TRUE_BOOLEAN) {
                // if the first one is true, replace with TRUE
                return new Node(Token.TRUE);
            } else if (leftStatus == ALWAYS_FALSE_BOOLEAN) {
                // if first is false, set to second
                return right;
            }
            int rightStatus = isAlwaysDefinedBoolean(right);
            if (rightStatus == ALWAYS_TRUE_BOOLEAN) {
                // if the second one is true, replace with TRUE
                if (!hasSideEffects(left)) {
                    return new Node(Token.TRUE);
                }
            } else if (rightStatus == ALWAYS_FALSE_BOOLEAN) {
                // if second is false, set to first
                return left;
            }
            break;
          }
        }

        return new Node(nodeType, left, right);
    }

    Node createAssignment(Node left, Node right)
    {
        int nodeType = left.getType();
        switch (nodeType) {
          case Token.NAME:
            left.setType(Token.BINDNAME);
            return new Node(Token.SETNAME, left, right);

          case Token.GETPROP:
          case Token.GETELEM: {
            Node obj = left.getFirstChild();
            Node id = left.getLastChild();
            int type;
            if (nodeType == Token.GETPROP) {
                type = Token.SETPROP;
            } else {
                type = Token.SETELEM;
            }
            return new Node(type, obj, id, right);
          }
          case Token.GET_REF: {
            Node ref = left.getFirstChild();
            return new Node(Token.SET_REF, ref, right);
          }
        }
        return createAssignment(makeReferenceGet(left), right);
    }

    Node createAssignmentOp(int assignOp, Node left, Node right)
    {
        int nodeType = left.getType();
        switch (nodeType) {
          case Token.NAME: {
            String s = left.getString();

            Node opLeft = Node.newString(Token.NAME, s);
            Node op = new Node(assignOp, opLeft, right);
            Node lvalueLeft = Node.newString(Token.BINDNAME, s);
            return new Node(Token.SETNAME, lvalueLeft, op);
          }
          case Token.GETPROP:
          case Token.GETELEM: {
            Node obj = left.getFirstChild();
            Node id = left.getLastChild();

            int type = nodeType == Token.GETPROP
                       ? Token.SETPROP_OP
                       : Token.SETELEM_OP;

            Node opLeft = new Node(Token.USE_STACK);
            Node op = new Node(assignOp, opLeft, right);
            return new Node(type, obj, id, op);
          }
          case Token.GET_REF: {
            Node ref = left.getFirstChild();
            Node opLeft = new Node(Token.USE_STACK);
            Node op = new Node(assignOp, opLeft, right);
            return new Node(Token.SET_REF_OP, ref, op);
          }
        }
        return createAssignmentOp(assignOp, makeReferenceGet(left), right);
    }

    Node createUseLocal(Node localBlock)
    {
        if (Token.LOCAL_BLOCK != localBlock.getType()) throw Kit.codeBug();
        Node result = new Node(Token.LOCAL_LOAD);
        result.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
        return result;
    }

    // Check if Node always mean true or false in boolean context
    private static int isAlwaysDefinedBoolean(Node node)
    {
        switch (node.getType()) {
          case Token.FALSE:
          case Token.NULL:
            return ALWAYS_FALSE_BOOLEAN;
          case Token.TRUE:
            return ALWAYS_TRUE_BOOLEAN;
          case Token.NUMBER: {
            double num = node.getDouble();
            if (num == num && num != 0.0) {
                return ALWAYS_TRUE_BOOLEAN;
            } else {
                return ALWAYS_FALSE_BOOLEAN;
            }
          }
        }
        return 0;
    }

    private static boolean hasSideEffects(Node exprTree)
    {
        switch (exprTree.getType()) {
            case Token.INC:
            case Token.DEC:
            case Token.SETPROP:
            case Token.SETELEM:
            case Token.SETNAME:
            case Token.CALL:
            case Token.NEW:
                return true;
            default:
                Node child = exprTree.getFirstChild();
                while (child != null) {
                    if (hasSideEffects(child))
                        return true;
                    child = child.getNext();
                }
                break;
        }
        return false;
    }

    private void checkActivationName(String name, int token)
    {
        if (parser.insideFunction()) {
            boolean activation = false;
            if ("arguments".equals(name)
                || (parser.compilerEnv.activationNames != null
                    && parser.compilerEnv.activationNames.containsKey(name)))
            {
                activation = true;
            } else if ("length".equals(name)) {
                if (token == Token.GETPROP
                    && parser.compilerEnv.getLanguageVersion()
                       == Context.VERSION_1_2)
                {
                    // Use of "length" in 1.2 requires an activation object.
                    activation = true;
                }
            }
            if (activation) {
                setRequiresActivation();
            }
        }
    }

    private void setRequiresActivation()
    {
        if (parser.insideFunction()) {
            ((FunctionNode)parser.currentScriptOrFn).setRequiresActivation();
        }
    }

    // Only needed to call reportCurrentLineError.
    private Parser parser;

    private static final int LOOP_DO_WHILE = 0;
    private static final int LOOP_WHILE    = 1;
    private static final int LOOP_FOR      = 2;

    private static final int ALWAYS_TRUE_BOOLEAN = 1;
    private static final int ALWAYS_FALSE_BOOLEAN = -1;
}

