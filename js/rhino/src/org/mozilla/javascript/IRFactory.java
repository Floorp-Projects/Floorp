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
class IRFactory
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
    void initScript(ScriptOrFnNode scriptNode, Object body)
    {
        Node children = ((Node) body).getFirstChild();
        if (children != null) { scriptNode.addChildrenToBack(children); }
    }

    /**
     * Leaf
     */
    Object createLeaf(int nodeType)
    {
        return new Node(nodeType);
    }

    Object createLeaf(int nodeType, int nodeOp)
    {
        return new Node(nodeType, nodeOp);
    }

    /**
     * Statement leaf nodes.
     */

    Object createSwitch(int lineno)
    {
        return new Node.Jump(Token.SWITCH, lineno);
    }

    Object createVariables(int lineno)
    {
        return new Node(Token.VAR, lineno);
    }

    Object createExprStatement(Object expr, int lineno)
    {
        return new Node(Token.EXPRSTMT, (Node) expr, lineno);
    }

    Object createExprStatementNoReturn(Object expr, int lineno)
    {
        return new Node(Token.POP, (Node) expr, lineno);
    }

    /**
     * Name
     */
    Object createName(String name)
    {
        checkActivationName(name, Token.NAME);
        return Node.newString(Token.NAME, name);
    }

    /**
     * String (for literals)
     */
    Object createString(String string)
    {
        return Node.newString(string);
    }

    /**
     * Number (for literals)
     */
    Object createNumber(double number)
    {
        return Node.newNumber(number);
    }

    /**
     * Catch clause of try/catch/finally
     * @param varName the name of the variable to bind to the exception
     * @param catchCond the condition under which to catch the exception.
     *                  May be null if no condition is given.
     * @param stmts the statements in the catch clause
     * @param lineno the starting line number of the catch clause
     */
    Object createCatch(String varName, Object catchCond, Object stmts,
                       int lineno)
    {
        if (catchCond == null) {
            catchCond = new Node(Token.EMPTY);
        }
        return new Node(Token.CATCH, (Node)createName(varName),
                               (Node)catchCond, (Node)stmts, lineno);
    }

    /**
     * Throw
     */
    Object createThrow(Object expr, int lineno)
    {
        return new Node(Token.THROW, (Node)expr, lineno);
    }

    /**
     * Return
     */
    Object createReturn(Object expr, int lineno)
    {
        return expr == null
            ? new Node(Token.RETURN, lineno)
            : new Node(Token.RETURN, (Node)expr, lineno);
    }

    /**
     * Label
     */
    Object createLabel(String label, int lineno)
    {
        Node.Jump n = new Node.Jump(Token.LABEL, lineno);
        n.setLabel(label);
        return n;
    }

    /**
     * Break (possibly labeled)
     */
    Object createBreak(String label, int lineno)
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
    Object createContinue(String label, int lineno)
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
    Object createBlock(int lineno)
    {
        return new Node(Token.BLOCK, lineno);
    }

    FunctionNode createFunction(String name)
    {
        return new FunctionNode(name);
    }

    Object initFunction(FunctionNode fnNode, int functionIndex,
                        Object statements, int functionType)
    {
        Node stmts = (Node)statements;
        fnNode.setFunctionType(functionType);
        fnNode.addChildToBack(stmts);

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
                Node setFn = new Node(Token.POP,
                                new Node(Token.SETVAR, Node.newString(name),
                                         new Node(Token.THISFN)));
                stmts.addChildrenToFront(setFn);
            }
        }

        // Add return to end if needed.
        Node lastStmt = stmts.getLastChild();
        if (lastStmt == null || lastStmt.getType() != Token.RETURN) {
            stmts.addChildToBack(new Node(Token.RETURN));
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
    void addChildToBack(Object parent, Object child)
    {
        ((Node)parent).addChildToBack((Node)child);
    }

    /**
     * While
     */
    Object createWhile(Object cond, Object body, int lineno)
    {
        return createLoop(LOOP_WHILE, (Node)body, (Node)cond, null, null,
                          lineno);
    }

    /**
     * DoWhile
     */
    Object createDoWhile(Object body, Object cond, int lineno)
    {
        return createLoop(LOOP_DO_WHILE, (Node)body, (Node)cond, null, null,
                          lineno);
    }

    /**
     * For
     */
    Object createFor(Object init, Object test, Object incr, Object body,
                     int lineno)
    {
        return createLoop(LOOP_FOR, (Node)body, (Node)test,
                          (Node)init, (Node)incr, lineno);
    }

    private Node createLoop(int loopType, Node body, Node cond,
                            Node init, Node incr, int lineno)
    {
        Node.Target bodyTarget = new Node.Target();
        Node.Target condTarget = new Node.Target();
        if (loopType == LOOP_FOR && cond.getType() == Token.EMPTY) {
            cond = new Node(Token.TRUE);
        }
        Node.Jump IFEQ = new Node.Jump(Token.IFEQ, (Node)cond);
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
                        init = new Node(Token.POP, init);
                    }
                    result.addChildToFront(init);
                }
                Node.Target incrTarget = new Node.Target();
                result.addChildAfter(incrTarget, body);
                if (incr.getType() != Token.EMPTY) {
                    incr = (Node)createUnary(Token.POP, incr);
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
    Object createForIn(Object lhs, Object obj, Object body, int lineno)
    {
        String name;
        Node lhsNode = (Node) lhs;
        Node objNode = (Node) obj;
        int type = lhsNode.getType();

        Node lvalue = lhsNode;
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
            Node lastChild = lhsNode.getLastChild();
            if (lhsNode.getFirstChild() != lastChild) {
                parser.reportError("msg.mult.index");
            }
            lvalue = Node.newString(Token.NAME, lastChild.getString());
            break;

          default:
            parser.reportError("msg.bad.for.in.lhs");
            return objNode;
        }

        Node localBlock = new Node(Token.LOCAL_BLOCK);

        Node init = new Node(Token.ENUM_INIT, objNode);
        init.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
        Node cond = new Node(Token.ENUM_NEXT);
        cond.putProp(Node.LOCAL_BLOCK_PROP, localBlock);
        Node id = new Node(Token.ENUM_ID);
        id.putProp(Node.LOCAL_BLOCK_PROP, localBlock);

        Node newBody = new Node(Token.BLOCK);
        Node assign = (Node) createAssignment(lvalue, id);
        newBody.addChildToBack(new Node(Token.POP, assign));
        newBody.addChildToBack((Node) body);

        Node loop = (Node) createWhile(cond, newBody, lineno);
        loop.addChildToFront(init);
        if (type == Token.VAR)
            loop.addChildToFront(lhsNode);
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
    Object createTryCatchFinally(Object tryblock, Object catchblocks,
                                 Object finallyblock, int lineno)
    {
        Node trynode = (Node)tryblock;
        boolean hasFinally = false;
        Node finallyNode = null;
        if (finallyblock != null) {
            finallyNode = (Node)finallyblock;
            hasFinally = (finallyNode.getType() != Token.BLOCK
                          || finallyNode.hasChildren());
        }

        // short circuit
        if (trynode.getType() == Token.BLOCK && !trynode.hasChildren()
            && !hasFinally)
        {
            return trynode;
        }

        Node catchNodes = (Node)catchblocks;
        boolean hasCatch = catchNodes.hasChildren();

        // short circuit
        if (!hasFinally && !hasCatch)  {
            // bc finally might be an empty block...
            return trynode;
        }


        Node localBlock  = new Node(Token.LOCAL_BLOCK);
        Node.Jump pn = new Node.Jump(Token.TRY, trynode, lineno);
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
            // expects catchNode children to be (cond block) pairs.
            Node cb = catchNodes.getFirstChild();
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
                    condStmt = (Node) createIf(cond, catchBlock, null,
                                               catchLineNo);
                }
                // Try..catch produces "with" code in order to limit
                // the scope of the exception object.
                // OPT: We should be able to figure out the correct
                //      scoping at compile-time and avoid the
                //      runtime overhead.
                Node catchScope = Node.newString(Token.CATCH_SCOPE,
                                                 name.getString());
                catchScope.addChildToBack(createUseLocal(localBlock));
                Node withStmt = (Node) createWith(catchScope, condStmt,
                                                  catchLineNo);
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
            Node fBlock = new Node(Token.FINALLY, finallyNode);
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
    Object createWith(Object obj, Object body, int lineno)
    {
        setRequiresActivation();
        Node result = new Node(Token.BLOCK, lineno);
        result.addChildToBack(new Node(Token.ENTERWITH, (Node)obj));
        Node bodyNode = new Node(Token.WITH, (Node) body, lineno);
        result.addChildrenToBack(bodyNode);
        result.addChildToBack(new Node(Token.LEAVEWITH));
        return result;
    }

    Object createArrayLiteral(ObjArray elems, int skipCount)
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
    Object createObjectLiteral(ObjArray elems)
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
    Object createRegExp(int regexpIndex)
    {
        Node n = new Node(Token.REGEXP);
        n.putIntProp(Node.REGEXP_PROP, regexpIndex);
        return n;
    }

    /**
     * If statement
     */
    Object createIf(Object condObj, Object ifTrue, Object ifFalse, int lineno)
    {
        Node cond = (Node)condObj;
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
        Node.Jump IFNE = new Node.Jump(Token.IFNE, (Node) cond);
        IFNE.target = ifNotTarget;

        result.addChildToBack(IFNE);
        result.addChildrenToBack((Node)ifTrue);

        if (ifFalse != null) {
            Node.Jump GOTOToEnd = new Node.Jump(Token.GOTO);
            Node.Target endTarget = new Node.Target();
            GOTOToEnd.target = endTarget;
            result.addChildToBack(GOTOToEnd);
            result.addChildToBack(ifNotTarget);
            result.addChildrenToBack((Node)ifFalse);
            result.addChildToBack(endTarget);
        } else {
            result.addChildToBack(ifNotTarget);
        }

        return result;
    }

    Object createCondExpr(Object condObj, Object ifTrue, Object ifFalse)
    {
        Node cond = (Node)condObj;
        int condStatus = isAlwaysDefinedBoolean(cond);
        if (condStatus == ALWAYS_TRUE_BOOLEAN) {
            return ifTrue;
        } else if (condStatus == ALWAYS_FALSE_BOOLEAN) {
            return ifFalse;
        }
        return new Node(Token.HOOK, cond, (Node)ifTrue, (Node)ifFalse);
    }

    /**
     * Unary
     */
    Object createUnary(int nodeType, Object child)
    {
        Node childNode = (Node) child;
        int childType = childNode.getType();
        switch (nodeType) {
          case Token.DELPROP: {
            Node left;
            Node right;
            if (childType == Token.NAME) {
                // Transform Delete(Name "a")
                //  to Delete(Bind("a"), String("a"))
                childNode.setType(Token.BINDNAME);
                left = childNode;
                right = Node.newString(childNode.getString());
            } else if (childType == Token.GETPROP ||
                       childType == Token.GETELEM)
            {
                left = childNode.getFirstChild();
                right = childNode.getLastChild();
                childNode.removeChild(left);
                childNode.removeChild(right);
            } else {
                return new Node(Token.TRUE);
            }
            return new Node(nodeType, left, right);
          }
          case Token.TYPEOF:
            if (childType == Token.NAME) {
                childNode.setType(Token.TYPEOFNAME);
                return childNode;
            }
            break;
          case Token.BITNOT:
            if (childType == Token.NUMBER) {
                int value = ScriptRuntime.toInt32(childNode.getDouble());
                childNode.setDouble(~value);
                return childNode;
            }
            break;
          case Token.NEG:
            if (childType == Token.NUMBER) {
                childNode.setDouble(-childNode.getDouble());
                return childNode;
            }
            break;
          case Token.NOT: {
            int status = isAlwaysDefinedBoolean(childNode);
            if (status != 0) {
                int type;
                if (status == ALWAYS_TRUE_BOOLEAN) {
                    type = Token.FALSE;
                } else {
                    type = Token.TRUE;
                }
                if (childType == Token.TRUE || childType == Token.FALSE) {
                    childNode.setType(type);
                    return childNode;
                }
                return new Node(type);
            }
            break;
          }
        }
        return new Node(nodeType, childNode);
    }

    Object createCallOrNew(int nodeType, Object childNode)
    {
        Node child = (Node)childNode;
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

    Object createIncDec(int nodeType, boolean post, Object child)
    {
        Node childNode = (Node)child;
        int childType = childNode.getType();

        if (childType == Token.NAME
            || childType == Token.GETPROP
            || childType == Token.GETELEM
            || childType == Token.GET_REF)
        {
            Node n = new Node(nodeType, childNode);
            int type;
            if (nodeType == Token.INC) {
                type = (post) ? Node.POST_INC : Node.PRE_INC;
            } else {
                type = (post) ? Node.POST_DEC : Node.PRE_DEC;
            }
            n.putIntProp(Node.INCRDECR_PROP, type);
            return n;
        }
        // TODO: This should be a ReferenceError--but that's a runtime
        //  exception. Should we compile an exception into the code?
        parser.reportError("msg.bad.lhs.assign");
        return child;
    }

    /**
     * Binary
     */
    Object createBinary(int nodeType, Object leftObj, Object rightObj)
    {
        Node left = (Node)leftObj;
        Node right = (Node)rightObj;

        switch (nodeType) {

          case Token.DOT:
            nodeType = Token.GETPROP;
            right.setType(Token.STRING);
            String id = right.getString();
            int idlength = id.length();
            int special = 0;
            if (idlength == 9) {
                if (id.equals("__proto__")) {
                    special = Node.SPECIAL_PROP_PROTO;
                }
            } else if (idlength == 10) {
                if (id.equals("__parent__")) {
                    special = Node.SPECIAL_PROP_PARENT;
                }
            }
            if (special != 0) {
                Node ref = new Node(Token.SPECIAL_REF, left);
                ref.putIntProp(Node.SPECIAL_PROP_PROP, special);
                return new Node(Token.GET_REF, ref);
            }
            checkActivationName(id, Token.GETPROP);
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

    Object createAssignment(Object leftObj, Object rightObj)
    {
        Node left = (Node)leftObj;
        Node right = (Node)rightObj;

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

          default:
            // TODO: This should be a ReferenceError--but that's a runtime
            //  exception. Should we compile an exception into the code?
            parser.reportError("msg.bad.lhs.assign");
            return left;
        }
    }

    Object createAssignmentOp(int assignOp, Object leftObj, Object rightObj)
    {
        Node left = (Node)leftObj;
        Node right = (Node)rightObj;

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

          default:
            // TODO: This should be a ReferenceError--but that's a runtime
            //  exception. Should we compile an exception into the code?
            parser.reportError("msg.bad.lhs.assign");
            return left;
        }
    }

    Node createUseLocal(Node localBlock)
    {
        if (Token.LOCAL_BLOCK != localBlock.getType()) Kit.codeBug();
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
          case Token.UNDEFINED:
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
        boolean activation = false;
        if (parser.currentScriptOrFn.getType() == Token.FUNCTION) {
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
        }
        if (activation) {
            ((FunctionNode)parser.currentScriptOrFn).setRequiresActivation();
        }
    }

    private void setRequiresActivation()
    {
        if (parser.currentScriptOrFn.getType() == Token.FUNCTION) {
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

