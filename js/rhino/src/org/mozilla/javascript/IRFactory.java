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
public class IRFactory {

    public IRFactory(Interpreter compiler, TokenStream ts) {
        this.compiler = compiler;
        this.ts = ts;
    }

    public ScriptOrFnNode createScript() {
        return new ScriptOrFnNode(Token.SCRIPT);
    }

    /**
     * Script (for associating file/url names with toplevel scripts.)
     */
    public void initScript(ScriptOrFnNode scriptNode, Object body)
    {
        Node children = ((Node) body).getFirstChild();
        if (children != null) { scriptNode.addChildrenToBack(children); }
        scriptNode.finishParsing(this);
    }

    /**
     * Leaf
     */
    public Object createLeaf(int nodeType) {
        return new Node(nodeType);
    }

    public Object createLeaf(int nodeType, int nodeOp) {
        return new Node(nodeType, nodeOp);
    }

    /**
     * Statement leaf nodes.
     */

    public Object createSwitch(int lineno) {
        return new Node(Token.SWITCH, lineno);
    }

    public Object createVariables(int lineno) {
        return new Node(Token.VAR, lineno);
    }

    public Object createExprStatement(Object expr, int lineno) {
        return new Node(Token.EXPRSTMT, (Node) expr, lineno);
    }

    public Object createExprStatementNoReturn(Object expr, int lineno) {
        return new Node(Token.POP, (Node) expr, lineno);
    }

    /**
     * Name
     */
    public Object createName(String name) {
        return Node.newString(Token.NAME, name);
    }

    /**
     * String (for literals)
     */
    public Object createString(String string) {
        return Node.newString(string);
    }

    /**
     * Number (for literals)
     */
    public Object createNumber(double number) {
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
    public Object createCatch(String varName, Object catchCond, Object stmts,
                              int lineno)
    {
        if (catchCond == null) {
            catchCond = new Node(Token.TRUE);
        }
        return new Node(Token.CATCH, (Node)createName(varName),
                               (Node)catchCond, (Node)stmts, lineno);
    }

    /**
     * Throw
     */
    public Object createThrow(Object expr, int lineno) {
        return new Node(Token.THROW, (Node)expr, lineno);
    }

    /**
     * Return
     */
    public Object createReturn(Object expr, int lineno) {
        return expr == null
            ? new Node(Token.RETURN, lineno)
            : new Node(Token.RETURN, (Node)expr, lineno);
    }

    /**
     * Label
     */
    public Object createLabel(String label, int lineno) {
        Node result = new Node(Token.LABEL, lineno);
        Node name = Node.newString(Token.NAME, label);
        result.addChildToBack(name);
        return result;
    }

    /**
     * Break (possibly labeled)
     */
    public Object createBreak(String label, int lineno) {
        if (label == null) {
            return new Node(Token.BREAK, lineno);
        } else {
            Node name = Node.newString(Token.NAME, label);
            return new Node(Token.BREAK, name, lineno);
        }
    }

    /**
     * Continue (possibly labeled)
     */
    public Object createContinue(String label, int lineno) {
        if (label == null) {
            return new Node(Token.CONTINUE, lineno);
        } else {
            Node name = Node.newString(Token.NAME, label);
            return new Node(Token.CONTINUE, name, lineno);
        }
    }

    /**
     * Statement block
     * Creates the empty statement block
     * Must make subsequent calls to add statements to the node
     */
    public Object createBlock(int lineno) {
        return new Node(Token.BLOCK, lineno);
    }

    public FunctionNode createFunction(String name) {
        return compiler.createFunctionNode(this, name);
    }

    public Object initFunction(FunctionNode fnNode, int functionIndex,
                               Object statements, int functionType)
    {
        fnNode.setFunctionType(functionType);
        fnNode.addChildToBack((Node)statements);
        fnNode.finishParsing(this);

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
    public void addChildToBack(Object parent, Object child) {
        ((Node)parent).addChildToBack((Node)child);
    }

    /**
     * While
     */
    public Object createWhile(Object cond, Object body, int lineno) {
        return createLoop(LOOP_WHILE, (Node)body, (Node)cond, null, null,
                          lineno);
    }

    /**
     * DoWhile
     */
    public Object createDoWhile(Object body, Object cond, int lineno) {
        return createLoop(LOOP_DO_WHILE, (Node)body, (Node)cond, null, null,
                          lineno);
    }

    /**
     * For
     */
    public Object createFor(Object init, Object test, Object incr,
                            Object body, int lineno)
    {
        return createLoop(LOOP_FOR, (Node)body, (Node)test,
                          (Node)init, (Node)incr, lineno);
    }

    private Node createLoop(int loopType, Node body, Node cond,
                            Node init, Node incr, int lineno)
    {
        Node bodyTarget = new Node(Token.TARGET);
        Node condTarget = new Node(Token.TARGET);
        if (loopType == LOOP_FOR && cond.getType() == Token.EMPTY) {
            cond = new Node(Token.TRUE);
        }
        Node IFEQ = new Node(Token.IFEQ, (Node)cond);
        IFEQ.putProp(Node.TARGET_PROP, bodyTarget);
        Node breakTarget = new Node(Token.TARGET);

        Node result = new Node(Token.LOOP, lineno);
        result.addChildToBack(bodyTarget);
        result.addChildrenToBack(body);
        if (loopType == LOOP_WHILE || loopType == LOOP_FOR) {
            // propagate lineno to condition
            result.addChildrenToBack(new Node(Token.EMPTY, lineno));
        }
        result.addChildToBack(condTarget);
        result.addChildToBack(IFEQ);
        result.addChildToBack(breakTarget);

        result.putProp(Node.BREAK_PROP, breakTarget);
        Node continueTarget = condTarget;

        if (loopType == LOOP_WHILE || loopType == LOOP_FOR) {
            // Just add a GOTO to the condition in the do..while
            Node GOTO = new Node(Token.GOTO);
            GOTO.putProp(Node.TARGET_PROP, condTarget);
            result.addChildToFront(GOTO);

            if (loopType == LOOP_FOR) {
                if (init.getType() != Token.EMPTY) {
                    if (init.getType() != Token.VAR) {
                        init = new Node(Token.POP, init);
                    }
                    result.addChildToFront(init);
                }
                Node incrTarget = new Node(Token.TARGET);
                result.addChildAfter(incrTarget, body);
                if (incr.getType() != Token.EMPTY) {
                    incr = (Node)createUnary(Token.POP, incr);
                    result.addChildAfter(incr, incrTarget);
                }
                continueTarget = incrTarget;
            }
        }

        result.putProp(Node.CONTINUE_PROP, continueTarget);

        return result;
    }

    /**
     * For .. In
     *
     */
    public Object createForIn(Object lhs, Object obj, Object body, int lineno) {
        String name;
        Node lhsNode = (Node) lhs;
        Node objNode = (Node) obj;
        int type = lhsNode.getType();

        Node lvalue = lhsNode;
        switch (type) {

          case Token.NAME:
          case Token.GETPROP:
          case Token.GETELEM:
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
                ts.reportCurrentLineError("msg.mult.index", null);
            }
            lvalue = Node.newString(Token.NAME, lastChild.getString());
            break;

          default:
            ts.reportCurrentLineError("msg.bad.for.in.lhs", null);
            return objNode;
        }

        Node init = new Node(Token.ENUMINIT, objNode);
        Node next = new Node(Token.ENUMNEXT);
        next.putProp(Node.ENUM_PROP, init);
        Node temp = createNewTemp(next);
        Node cond = new Node(Token.NE);
        cond.addChildToBack(temp);
        cond.addChildToBack(new Node(Token.NULL));
        Node newBody = new Node(Token.BLOCK);
        Node assign = (Node) createAssignment(Token.NOP, lvalue,
                                              createUseTemp(temp));
        newBody.addChildToBack(new Node(Token.POP, assign));
        newBody.addChildToBack((Node) body);
        Node result = (Node) createWhile(cond, newBody, lineno);

        result.addChildToFront(init);
        if (type == Token.VAR)
            result.addChildToFront(lhsNode);

        Node done = new Node(Token.ENUMDONE);
        done.putProp(Node.ENUM_PROP, init);
        result.addChildToBack(done);

        return result;
    }

    /**
     * Try/Catch/Finally
     *
     * The IRFactory tries to express as much as possible in the tree;
     * the responsibilities remaining for Codegen are to add the Java
     * handlers: (Either (but not both) of TARGET and FINALLY might not
     * be defined)

     * - a catch handler for javascript exceptions that unwraps the
     * exception onto the stack and GOTOes to the catch target -
     * TARGET_PROP in the try node.

     * - a finally handler that catches any exception, stores it to a
     * temporary, and JSRs to the finally target - FINALLY_PROP in the
     * try node - before re-throwing the exception.

     * ... and a goto to GOTO around these handlers.
     */
    public Object createTryCatchFinally(Object tryblock, Object catchblocks,
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

        Node pn = new Node(Token.TRY, trynode, lineno);

        Node finallyTarget = null;
        if (hasFinally) {
            // make a TARGET for the finally that the tcf node knows about
            finallyTarget = new Node(Token.TARGET);
            pn.putProp(Node.FINALLY_PROP, finallyTarget);

            // add jsr finally to the try block
            Node jsrFinally = new Node(Token.JSR);
            jsrFinally.putProp(Node.TARGET_PROP, finallyTarget);
            pn.addChildToBack(jsrFinally);
        }

        Node endTarget = new Node(Token.TARGET);
        Node GOTOToEnd = new Node(Token.GOTO);
        GOTOToEnd.putProp(Node.TARGET_PROP, endTarget);
        pn.addChildToBack(GOTOToEnd);

        if (hasCatch) {
            /*
             *
               Given

                try {
                        throw 3;
                } catch (e: e instanceof Object) {
                        print("object");
                } catch (e2) {
                        print(e2);
                }

               rewrite as

                try {
                        throw 3;
                } catch (x) {
                        o = newScope();
                        o.e = x;
                        with (o) {
                                if (e instanceof Object) {
                                        print("object");
                                }
                        }
                        o2 = newScope();
                        o2.e2 = x;
                        with (o2) {
                                if (true) {
                                        print(e2);
                                }
                        }
                }
            */
            // make a TARGET for the catch that the tcf node knows about
            Node catchTarget = new Node(Token.TARGET);
            pn.putProp(Node.TARGET_PROP, catchTarget);
            // mark it
            pn.addChildToBack(catchTarget);

            // get the exception object and store it in a temp
            Node exn = createNewLocal(new Node(Token.EMPTY));
            pn.addChildToBack(new Node(Token.POP, exn));

            Node endCatch = new Node(Token.TARGET);

            // add [jsr finally?] goto end to each catch block
            // expects catchNode children to be (cond block) pairs.
            Node cb = catchNodes.getFirstChild();
            while (cb != null) {
                Node catchStmt = new Node(Token.BLOCK);
                int catchLineNo = cb.getLineno();

                Node name = cb.getFirstChild();
                Node cond = name.getNext();
                Node catchBlock = cond.getNext();
                cb.removeChild(name);
                cb.removeChild(cond);
                cb.removeChild(catchBlock);

                Node newScope = createNewLocal(new Node(Token.NEWSCOPE));
                Node initScope = new Node(Token.SETPROP, newScope,
                                          Node.newString(name.getString()),
                                          createUseLocal(exn));
                catchStmt.addChildToBack(new Node(Token.POP, initScope));

                catchBlock.addChildToBack(new Node(Token.LEAVEWITH));
                Node GOTOToEndCatch = new Node(Token.GOTO);
                GOTOToEndCatch.putProp(Node.TARGET_PROP, endCatch);
                catchBlock.addChildToBack(GOTOToEndCatch);

                Node ifStmt = (Node) createIf(cond, catchBlock, null, catchLineNo);
                // Try..catch produces "with" code in order to limit
                // the scope of the exception object.
                // OPT: We should be able to figure out the correct
                //      scoping at compile-time and avoid the
                //      runtime overhead.
                Node withStmt = (Node) createWith(createUseLocal(newScope),
                                                  ifStmt, catchLineNo);
                catchStmt.addChildToBack(withStmt);

                pn.addChildToBack(catchStmt);

                // move to next cb
                cb = cb.getNext();
            }

            // Generate code to rethrow if no catch clause was executed
            Node rethrow = new Node(Token.THROW, createUseLocal(exn));
            pn.addChildToBack(rethrow);

            pn.addChildToBack(endCatch);
            // add a JSR finally if needed
            if (hasFinally) {
                Node jsrFinally = new Node(Token.JSR);
                jsrFinally.putProp(Node.TARGET_PROP, finallyTarget);
                pn.addChildToBack(jsrFinally);
                Node GOTO = new Node(Token.GOTO);
                GOTO.putProp(Node.TARGET_PROP, endTarget);
                pn.addChildToBack(GOTO);
            }
        }

        if (hasFinally) {
            pn.addChildToBack(finallyTarget);
            pn.addChildToBack(new Node(Token.FINALLY, finallyNode));
        }
        pn.addChildToBack(endTarget);
        return pn;
    }

    /**
     * Throw, Return, Label, Break and Continue are defined in ASTFactory.
     */

    /**
     * With
     */
    public Object createWith(Object obj, Object body, int lineno) {
        Node result = new Node(Token.BLOCK, lineno);
        result.addChildToBack(new Node(Token.ENTERWITH, (Node)obj));
        Node bodyNode = new Node(Token.WITH, (Node) body, lineno);
        result.addChildrenToBack(bodyNode);
        result.addChildToBack(new Node(Token.LEAVEWITH));
        return result;
    }

    /**
     * Array Literal
     * <BR>createArrayLiteral rewrites its argument as array creation
     * plus a series of array element entries, so later compiler
     * stages don't need to know about array literals.
     */
    public Object createArrayLiteral(Object obj) {
        Node array;
        array = new Node(Token.NEW, Node.newString(Token.NAME, "Array"));
        Node temp = createNewTemp(array);

        Node elem = null;
        int i = 0;
        Node comma = new Node(Token.COMMA, temp);
        for (Node cursor = ((Node) obj).getFirstChild(); cursor != null;) {
            // Move cursor to cursor.next before elem.next can be
            // altered in new Node constructor
            elem = cursor;
            cursor = cursor.getNext();
            if (elem.getType() == Token.UNDEFINED) {
                i++;
                continue;
            }
            Node addelem = new Node(Token.SETELEM, createUseTemp(temp),
                                    Node.newNumber(i), elem);
            i++;
            comma.addChildToBack(addelem);
        }

        /*
         * If the version is 120, then new Array(4) means create a new
         * array with 4 as the first element.  In this case, we might
         * need to explicitly check against trailing undefined
         * elements in the array literal, and set the length manually
         * if these occur.  Otherwise, we can add an argument to the
         * node specifying new Array() to provide the array length.
         * (Which will make Array optimizations involving allocating a
         * Java array to back the javascript array work better.)
         */
        if (Context.getContext().getLanguageVersion() == Context.VERSION_1_2) {
            /* When last array element is empty, we need to set the
             * length explicitly, because we can't depend on SETELEM
             * to do it for us - because empty [,,] array elements
             * never set anything at all. */
            if (elem != null && elem.getType() == Token.UNDEFINED) {
                Node setlength = new Node(Token.SETPROP,
                                          createUseTemp(temp),
                                          Node.newString("length"),
                                          Node.newNumber(i));
                comma.addChildToBack(setlength);
            }
        } else {
            array.addChildToBack(Node.newNumber(i));
        }
        comma.addChildToBack(createUseTemp(temp));
        return comma;
    }

    /**
     * Object Literals
     * <BR> createObjectLiteral rewrites its argument as object
     * creation plus object property entries, so later compiler
     * stages don't need to know about object literals.
     */
    public Object createObjectLiteral(Object obj) {
        Node result = new Node(Token.NEW, Node.newString(Token.NAME,
                                                         "Object"));
        Node temp = createNewTemp(result);

        Node comma = new Node(Token.COMMA, temp);
        for (Node cursor = ((Node) obj).getFirstChild(); cursor != null;) {
            Node n = cursor;
            cursor = cursor.getNext();
            int op = (n.getType() == Token.NAME)
                   ? Token.SETPROP
                   : Token.SETELEM;
            // Move cursor before next.next can be altered in new Node
            Node next = cursor;
            cursor = cursor.getNext();
            Node addelem = new Node(op, createUseTemp(temp), n, next);
            comma.addChildToBack(addelem);
        }
        comma.addChildToBack(createUseTemp(temp));
        return comma;
    }

    /**
     * Regular expressions
     */
    public Object createRegExp(int regexpIndex) {
        Node n = new Node(Token.REGEXP);
        n.putIntProp(Node.REGEXP_PROP, regexpIndex);
        return n;
    }

    /**
     * If statement
     */
    public Object createIf(Object cond, Object ifTrue, Object ifFalse,
                           int lineno)
    {
        Node result = new Node(Token.BLOCK, lineno);
        Node ifNotTarget = new Node(Token.TARGET);
        Node IFNE = new Node(Token.IFNE, (Node) cond);
        IFNE.putProp(Node.TARGET_PROP, ifNotTarget);

        result.addChildToBack(IFNE);
        result.addChildrenToBack((Node)ifTrue);

        if (ifFalse != null) {
            Node GOTOToEnd = new Node(Token.GOTO);
            Node endTarget = new Node(Token.TARGET);
            GOTOToEnd.putProp(Node.TARGET_PROP, endTarget);
            result.addChildToBack(GOTOToEnd);
            result.addChildToBack(ifNotTarget);
            result.addChildrenToBack((Node)ifFalse);
            result.addChildToBack(endTarget);
        } else {
            result.addChildToBack(ifNotTarget);
        }

        return result;
    }

    public Object createTernary(Object cond, Object ifTrue, Object ifFalse) {
        return createIf(cond, ifTrue, ifFalse, -1);
    }

    /**
     * Unary
     */
    public Object createUnary(int nodeType, Object child) {
        Node childNode = (Node) child;
        if (nodeType == Token.DELPROP) {
            int childType = childNode.getType();
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

        } else if (nodeType == Token.TYPEOF) {
            if (childNode.getType() == Token.NAME) {
                childNode.setType(Token.TYPEOFNAME);
                return childNode;
            }
        }
        return new Node(nodeType, childNode);
    }

    public Object createIncDec(int nodeType, boolean post, Object child)
    {
        Node childNode = (Node)child;
        int childType = childNode.getType();

        if (post && !hasSideEffects(childNode)
            && (childType == Token.NAME
                || childType == Token.GETPROP
                || childType == Token.GETELEM))
        {
            // if it's not a LHS type, createAssignment (below) will throw
            // an exception.
            return new Node(nodeType, childNode);
        }

        /*
         * Transform INC/DEC ops to +=1, -=1,
         * expecting later optimization of all +/-=1 cases to INC, DEC.
         */
        // we have to use Double for now, because
        // 0.0 and 1.0 are stored as dconst_[01],
        // and using a Float creates a stack mismatch.
        Node rhs = (Node) createNumber(1.0);

        return createAssignment(nodeType == Token.INC
                                    ? Token.ADD
                                    : Token.SUB,
                                childNode,
                                rhs,
                                true,
                                post);
    }

    /**
     * Binary
     */
    public Object createBinary(int nodeType, Object leftObj, Object rightObj)
    {
        Node left = (Node)leftObj;
        Node right = (Node)rightObj;

        switch (nodeType) {

          case Token.DOT:
            nodeType = Token.GETPROP;
            right.setType(Token.STRING);
            String id = right.getString();
            if (id.equals("__proto__") || id.equals("__parent__")) {
                Node result = new Node(nodeType, left);
                result.putProp(Node.SPECIAL_PROP_PROP, id);
                return result;
            }
            break;

          case Token.LB:
            // OPT: could optimize to GETPROP iff string can't be a number
            nodeType = Token.GETELEM;
            break;
        }
        return new Node(nodeType, left, right);
    }

    public Object createAssignment(int assignOp, Object left, Object right)
    {
        return createAssignment(assignOp, (Node) left, (Node) right,
                                false, false);
    }

    private Node createAssignment(int assignOp, Node left, Node right,
                                  boolean tonumber, boolean postfix)
    {
        int nodeType = left.getType();
        String idString;
        Node id = null;
        switch (nodeType) {
          case Token.NAME:
            return createSetName(assignOp, left, right, tonumber, postfix);

          case Token.GETPROP:
            idString = (String) left.getProp(Node.SPECIAL_PROP_PROP);
            if (idString != null)
                id = Node.newString(idString);
            /* fall through */
          case Token.GETELEM:
            if (id == null)
                id = left.getLastChild();
            return createSetProp(nodeType, assignOp, left.getFirstChild(),
                                 id, right, tonumber, postfix);
          default:
            // TODO: This should be a ReferenceError--but that's a runtime
            //  exception. Should we compile an exception into the code?
            ts.reportCurrentLineError("msg.bad.lhs.assign", null);
            return left;
        }
    }

    private Node createSetName(int assignOp, Node left, Node right,
                               boolean tonumber, boolean postfix)
    {
        if (assignOp == Token.NOP) {
            left.setType(Token.BINDNAME);
            return new Node(Token.SETNAME, left, right);
        }

        String s = left.getString();

        if (s.equals("__proto__") || s.equals("__parent__")) {
            Node result = new Node(Token.SETPROP, left, right);
            result.putProp(Node.SPECIAL_PROP_PROP, s);
            return result;
        }

        Node opLeft = Node.newString(Token.NAME, s);
        if (tonumber)
            opLeft = new Node(Token.POS, opLeft);

        if (!postfix) {
            Node op = new Node(assignOp, opLeft, right);
            Node lvalueLeft = Node.newString(Token.BINDNAME, s);
            return new Node(Token.SETNAME, lvalueLeft, op);
        } else {
            opLeft = createNewTemp(opLeft);
            Node op = new Node(assignOp, opLeft, right);
            Node lvalueLeft = Node.newString(Token.BINDNAME, s);
            Node result = new Node(Token.SETNAME, lvalueLeft, op);
            result = new Node(Token.COMMA, result, createUseTemp(opLeft));
            return result;
        }
    }

    public Node createNewTemp(Node n) {
        int type = n.getType();
        if (type == Token.STRING || type == Token.NUMBER) {
            // Optimization: clone these values rather than storing
            // and loading from a temp
            return n;
        }
        return new Node(Token.NEWTEMP, n);
    }

    public Node createUseTemp(Node newTemp)
    {
        switch (newTemp.getType()) {
          case Token.NEWTEMP: {
            Node result = new Node(Token.USETEMP);
            result.putProp(Node.TEMP_PROP, newTemp);
            int n = newTemp.getIntProp(Node.USES_PROP, 0);
            if (n != Integer.MAX_VALUE) {
                newTemp.putIntProp(Node.USES_PROP, n + 1);
            }
            return result;
          }
          case Token.STRING:
            return Node.newString(newTemp.getString());
          case Token.NUMBER:
            return Node.newNumber(newTemp.getDouble());
          default:
            throw Context.codeBug();
        }
    }

    public Node createNewLocal(Node n) {
        return new Node(Token.NEWLOCAL, n);
    }

    public Node createUseLocal(Node newLocal) {
        if (Token.NEWLOCAL != newLocal.getType()) Context.codeBug();
        Node result = new Node(Token.USELOCAL);
        result.putProp(Node.LOCAL_PROP, newLocal);
        return result;
    }

    public static boolean hasSideEffects(Node exprTree) {
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

    private Node createSetProp(int nodeType, int assignOp, Node obj, Node id,
                               Node expr, boolean tonumber, boolean postfix)
    {
        int type = nodeType == Token.GETPROP
                   ? Token.SETPROP
                   : Token.SETELEM;

        if (type == Token.SETPROP) {
            String s = id.getString();
            if (s != null && s.equals("__proto__") || s.equals("__parent__")) {
                Node result = new Node(type, obj, expr);
                result.putProp(Node.SPECIAL_PROP_PROP, s);
                return result;
            }
        }

        if (assignOp == Token.NOP)
            return new Node(type, obj, id, expr);
/*
*    If the RHS expression could modify the LHS we have
*    to construct a temporary to hold the LHS context
*    prior to running the expression. Ditto, if the id
*    expression has side-effects.
*
*    XXX If the hasSideEffects tests take too long, we
*       could make this an optimizer-only transform
*       and always do the temp assignment otherwise.
*
*/
        Node tmp1, tmp2, opLeft;
        if (obj.getType() != Token.NAME || id.hasChildren() ||
            hasSideEffects(expr) || hasSideEffects(id))
        {
            tmp1 = createNewTemp(obj);
            Node useTmp1 = createUseTemp(tmp1);

            tmp2 = createNewTemp(id);
            Node useTmp2 = createUseTemp(tmp2);

            opLeft = new Node(nodeType, useTmp1, useTmp2);
        } else {
            tmp1 = Node.newString(Token.NAME, obj.getString());
            tmp2 = id.cloneNode();
            opLeft = new Node(nodeType, obj, id);
        }

        if (tonumber)
            opLeft = new Node(Token.POS, opLeft);

        if (!postfix) {
            Node op = new Node(assignOp, opLeft, expr);
            return new Node(type, tmp1, tmp2, op);
        } else {
            opLeft = createNewTemp(opLeft);
            Node op = new Node(assignOp, opLeft, expr);
            Node result = new Node(type, tmp1, tmp2, op);
            result = new Node(Token.COMMA, result, createUseTemp(opLeft));
            return result;
        }
    }

    private Interpreter compiler;

    // Only needed to call reportSyntaxError. Could create an interface
    // that TokenStream implements if we want to make the connection less
    // direct.
    TokenStream ts;

    private static final int LOOP_DO_WHILE = 0;
    private static final int LOOP_WHILE    = 1;
    private static final int LOOP_FOR      = 2;
}

