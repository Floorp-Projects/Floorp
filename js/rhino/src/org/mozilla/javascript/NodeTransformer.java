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
 * Roger Lawrence
 * Mike McCabe
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
 * This class transforms a tree to a lower-level representation for codegen.
 *
 * @see Node
 * @author Norris Boyd
 */

public class NodeTransformer
{

    public NodeTransformer()
    {
    }

    public final void transform(ScriptOrFnNode tree)
    {
        transformCompilationUnit(tree);
        for (int i = 0; i != tree.getFunctionCount(); ++i) {
            FunctionNode fn = tree.getFunctionNode(i);
            transform(fn);
        }
    }

    private void transformCompilationUnit(ScriptOrFnNode tree)
    {
        loops = new ObjArray();
        loopEnds = new ObjArray();
        inFunction = (tree.getType() == Token.FUNCTION);
        // to save against upchecks if no finally blocks are used.
        hasFinally = false;
        transformCompilationUnit_r(tree, tree);
    }

    private void transformCompilationUnit_r(final ScriptOrFnNode tree,
                                            final Node parent)
    {
        Node node = null;
      siblingLoop:
        for (;;) {
            Node previous = null;
            if (node == null) {
                node = parent.getFirstChild();
            } else {
                previous = node;
                node = node.getNext();
            }
            if (node == null) {
                break;
            }

            int type = node.getType();

            switch (type) {

              case Token.LABEL:
              case Token.SWITCH:
              case Token.LOOP:
                loops.push(node);
                loopEnds.push(((Node.Jump)node).target);
                break;

              case Token.WITH:
              {
                loops.push(node);
                Node leave = node.getNext();
                if (leave.getType() != Token.LEAVEWITH) {
                    Kit.codeBug();
                }
                loopEnds.push(leave);
                break;
              }

              case Token.TRY:
              {
                Node.Jump jump = (Node.Jump)node;
                Node finallytarget = jump.getFinally();
                if (finallytarget != null) {
                    hasFinally = true;
                    loops.push(node);
                    loopEnds.push(finallytarget);
                }
                break;
              }

              case Token.TARGET:
              case Token.LEAVEWITH:
                if (!loopEnds.isEmpty() && loopEnds.peek() == node) {
                    loopEnds.pop();
                    loops.pop();
                }
                break;

              case Token.RETURN:
              {
                /* If we didn't support try/finally, it wouldn't be
                 * necessary to put LEAVEWITH nodes here... but as
                 * we do need a series of JSR FINALLY nodes before
                 * each RETURN, we need to ensure that each finally
                 * block gets the correct scope... which could mean
                 * that some LEAVEWITH nodes are necessary.
                 */
                if (!hasFinally)
                    break;     // skip the whole mess.
                Node unwindBlock = null;
                for (int i=loops.size()-1; i >= 0; i--) {
                    Node n = (Node) loops.get(i);
                    int elemtype = n.getType();
                    if (elemtype == Token.TRY || elemtype == Token.WITH) {
                        Node unwind;
                        if (elemtype == Token.TRY) {
                            Node.Jump jsrnode = new Node.Jump(Token.JSR);
                            Node jsrtarget = ((Node.Jump)n).getFinally();
                            jsrnode.target = jsrtarget;
                            unwind = jsrnode;
                        } else {
                            unwind = new Node(Token.LEAVEWITH);
                        }
                        if (unwindBlock == null) {
                            unwindBlock = new Node(Token.BLOCK,
                                                   node.getLineno());
                        }
                        unwindBlock.addChildToBack(unwind);
                    }
                }
                if (unwindBlock != null) {
                    Node returnNode = node;
                    Node returnExpr = returnNode.getFirstChild();
                    node = replaceCurrent(parent, previous, node, unwindBlock);
                    if (returnExpr == null) {
                        unwindBlock.addChildToBack(returnNode);
                    } else {
                        Node store = new Node(Token.EXPR_RESULT, returnExpr);
                        unwindBlock.addChildToFront(store);
                        returnNode = new Node(Token.RETURN_RESULT);
                        unwindBlock.addChildToBack(returnNode);
                        // transform return expression
                        transformCompilationUnit_r(tree, store);
                    }
                    // skip transformCompilationUnit_r to avoid infinite loop
                    continue siblingLoop;
                }
                break;
              }

              case Token.BREAK:
              case Token.CONTINUE:
              {
                Node.Jump jump = (Node.Jump)node;
                Node.Jump jumpStatement = jump.getJumpStatement();
                if (jumpStatement == null) Kit.codeBug();

                for (int i = loops.size(); ;) {
                    if (i == 0) {
                        // Parser/IRFactory ensure that break/continue
                        // always has a jump statement associated with it
                        // which should be found
                        throw Kit.codeBug();
                    }
                    --i;
                    Node n = (Node) loops.get(i);
                    if (n == jumpStatement) {
                        break;
                    }

                    int elemtype = n.getType();
                    if (elemtype == Token.WITH) {
                        Node leave = new Node(Token.LEAVEWITH);
                        previous = addBeforeCurrent(parent, previous, node,
                                                    leave);
                    } else if (elemtype == Token.TRY) {
                        Node.Jump tryNode = (Node.Jump)n;
                        Node.Jump jsrFinally = new Node.Jump(Token.JSR);
                        jsrFinally.target = tryNode.getFinally();
                        previous = addBeforeCurrent(parent, previous, node,
                                                    jsrFinally);
                    }
                }

                if (type == Token.BREAK) {
                    jump.target = jumpStatement.target;
                } else {
                    jump.target = jumpStatement.getContinue();
                }
                jump.setType(Token.GOTO);

                break;
              }

              case Token.CALL:
                visitCall(node, tree);
                break;

              case Token.NEW:
                visitNew(node, tree);
                break;

              case Token.VAR:
              {
                Node result = new Node(Token.BLOCK);
                for (Node cursor = node.getFirstChild(); cursor != null;) {
                    // Move cursor to next before createAssignment get chance
                    // to change n.next
                    Node n = cursor;
                    if (n.getType() != Token.NAME) Kit.codeBug();
                    cursor = cursor.getNext();
                    if (!n.hasChildren())
                        continue;
                    Node init = n.getFirstChild();
                    n.removeChild(init);
                    n.setType(Token.BINDNAME);
                    n = new Node(Token.SETNAME, n, init);
                    Node pop = new Node(Token.EXPR_VOID, n, node.getLineno());
                    result.addChildToBack(pop);
                }
                node = replaceCurrent(parent, previous, node, result);
                break;
              }

              case Token.DELPROP:
              case Token.SETNAME:
              {
                if (!inFunction || inWithStatement())
                    break;
                Node bind = node.getFirstChild();
                if (bind == null || bind.getType() != Token.BINDNAME)
                    break;
                String name = bind.getString();
                if (tree.hasParamOrVar(name)) {
                    if (type == Token.SETNAME) {
                        node.setType(Token.SETVAR);
                        bind.setType(Token.STRING);
                    } else {
                        // Local variables are by definition permanent
                        Node n = new Node(Token.FALSE);
                        node = replaceCurrent(parent, previous, node, n);
                    }
                }
                break;
              }

              case Token.NAME:
              {
                if (!inFunction || inWithStatement())
                    break;
                String name = node.getString();
                if (tree.hasParamOrVar(name)) {
                    node.setType(Token.GETVAR);
                }
                break;
              }
            }

            transformCompilationUnit_r(tree, node);
        }
    }

    protected void visitNew(Node node, ScriptOrFnNode tree) {
    }

    protected void visitCall(Node node, ScriptOrFnNode tree) {
    }

    protected boolean inWithStatement() {
        for (int i=loops.size()-1; i >= 0; i--) {
            Node n = (Node) loops.get(i);
            if (n.getType() == Token.WITH)
                return true;
        }
        return false;
    }

    private static Node addBeforeCurrent(Node parent, Node previous,
                                         Node current, Node toAdd)
    {
        if (previous == null) {
            if (!(current == parent.getFirstChild())) Kit.codeBug();
            parent.addChildToFront(toAdd);
        } else {
            if (!(current == previous.getNext())) Kit.codeBug();
            parent.addChildAfter(toAdd, previous);
        }
        return toAdd;
    }

    private static Node replaceCurrent(Node parent, Node previous,
                                       Node current, Node replacement)
    {
        if (previous == null) {
            if (!(current == parent.getFirstChild())) Kit.codeBug();
            parent.replaceChild(current, replacement);
        } else if (previous.next == current) {
            // Check cachedPrev.next == current is necessary due to possible
            // tree mutations
            parent.replaceChildAfter(previous, replacement);
        } else {
            parent.replaceChild(current, replacement);
        }
        return replacement;
    }

    private ObjArray loops;
    private ObjArray loopEnds;
    private boolean inFunction;
    private boolean hasFinally;
}

