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

public class NodeTransformer {

    public NodeTransformer(IRFactory irFactory) {
        this.irFactory = irFactory;
    }

    /**
     * Return new instance of this class. So that derived classes
     * can override methods of the transformer.
     */
    protected NodeTransformer newInstance() {
        return new NodeTransformer(irFactory);
    }

    public void transform(ScriptOrFnNode tree)
    {
        loops = new ObjArray();
        loopEnds = new ObjArray();
        inFunction = tree.getType() == TokenStream.FUNCTION;

        // to save against upchecks if no finally blocks are used.
        boolean hasFinally = false;

        PreorderNodeIterator iter = new PreorderNodeIterator();
        for (iter.start(tree); !iter.done(); iter.next()) {
            Node node = iter.getCurrent();
            int type = node.getType();

          typeswitch:
            switch (type) {

              case TokenStream.FUNCTION:
                if (node != tree) {
                    int fnIndex = node.getExistingIntProp(Node.FUNCTION_PROP);
                    FunctionNode fnNode = tree.getFunctionNode(fnIndex);
                    NodeTransformer inner = newInstance();
                    inner.transform(fnNode);
                }
                break;

              case TokenStream.LABEL:
              {
                Node child = node.getFirstChild();
                node.removeChild(child);
                String id = child.getString();

                // check against duplicate labels...
                for (int i=loops.size()-1; i >= 0; i--) {
                    Node n = (Node) loops.get(i);
                    if (n.getType() == TokenStream.LABEL) {
                        String otherId = (String)n.getProp(Node.LABEL_PROP);
                        if (id.equals(otherId)) {
                            Object[] messageArgs = { id };
                            reportError("msg.dup.label", messageArgs,
                                        node, tree);
                            break typeswitch;
                        }
                    }
                }

                node.putProp(Node.LABEL_PROP, id);

                /* Make a target and put it _after_ the following
                 * node.  And in the LABEL node, so breaks get the
                 * right target.
                 */
                Node breakTarget = new Node(TokenStream.TARGET);
                Node parent = iter.getCurrentParent();
                Node next = node.getNext();
                while (next != null &&
                       (next.getType() == TokenStream.LABEL ||
                        next.getType() == TokenStream.TARGET))
                    next = next.getNext();
                if (next == null)
                    break;
                parent.addChildAfter(breakTarget, next);
                node.putProp(Node.BREAK_PROP, breakTarget);

                if (next.getType() == TokenStream.LOOP) {
                    node.putProp(Node.CONTINUE_PROP,
                                 next.getProp(Node.CONTINUE_PROP));
                }

                loops.push(node);
                loopEnds.push(breakTarget);

                break;
              }

              case TokenStream.SWITCH:
              {
                Node breakTarget = new Node(TokenStream.TARGET);
                Node parent = iter.getCurrentParent();
                parent.addChildAfter(breakTarget, node);

                // make all children siblings except for selector
                Node sib = node;
                Node child = node.getFirstChild().next;
                while (child != null) {
                    Node next = child.next;
                    node.removeChild(child);
                    parent.addChildAfter(child, sib);
                    sib = child;
                    child = next;
                }

                node.putProp(Node.BREAK_PROP, breakTarget);
                loops.push(node);
                loopEnds.push(breakTarget);
                node.putProp(Node.CASES_PROP, new ObjArray());
                break;
              }

              case TokenStream.DEFAULT:
              case TokenStream.CASE:
              {
                Node sw = (Node) loops.peek();
                if (type == TokenStream.CASE) {
                    ObjArray cases = (ObjArray) sw.getProp(Node.CASES_PROP);
                    cases.add(node);
                } else {
                    sw.putProp(Node.DEFAULT_PROP, node);
                }
                break;
              }

              case TokenStream.NEWLOCAL :
                  tree.incrementLocalCount();
                break;

              case TokenStream.LOOP:
                loops.push(node);
                loopEnds.push(node.getProp(Node.BREAK_PROP));
                break;

              case TokenStream.WITH:
              {
                if (inFunction) {
                    // With statements require an activation object.
                    ((FunctionNode) tree).setRequiresActivation(true);
                }
                loops.push(node);
                Node leave = node.getNext();
                if (leave.getType() != TokenStream.LEAVEWITH) {
                    throw new RuntimeException("Unexpected tree");
                }
                loopEnds.push(leave);
                break;
              }

              case TokenStream.TRY:
              {
                Node finallytarget = (Node)node.getProp(Node.FINALLY_PROP);
                if (finallytarget != null) {
                    hasFinally = true;
                    loops.push(node);
                    loopEnds.push(finallytarget);
                }
                tree.incrementLocalCount();
                break;
              }

              case TokenStream.TARGET:
              case TokenStream.LEAVEWITH:
                if (!loopEnds.isEmpty() && loopEnds.peek() == node) {
                    loopEnds.pop();
                    loops.pop();
                }
                break;

              case TokenStream.RETURN:
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

                for (int i=loops.size()-1; i >= 0; i--) {
                    Node n = (Node) loops.get(i);
                    int elemtype = n.getType();
                    if (elemtype == TokenStream.TRY) {
                        Node jsrnode = new Node(TokenStream.JSR);
                        Object jsrtarget = n.getProp(Node.FINALLY_PROP);
                        jsrnode.putProp(Node.TARGET_PROP, jsrtarget);
                        iter.addBeforeCurrent(jsrnode);
                    } else if (elemtype == TokenStream.WITH) {
                        Node leave = new Node(TokenStream.LEAVEWITH);
                        iter.addBeforeCurrent(leave);
                    }
                }
                break;
              }

              case TokenStream.BREAK:
              case TokenStream.CONTINUE:
              {
                Node loop = null;
                boolean labelled = node.hasChildren();
                String id = null;
                if (labelled) {
                    /* get the label */
                    Node child = node.getFirstChild();
                    id = child.getString();
                    node.removeChild(child);
                }

                int i;
                for (i=loops.size()-1; i >= 0; i--) {
                    Node n = (Node) loops.get(i);
                    int elemtype = n.getType();
                    if (elemtype == TokenStream.WITH) {
                        Node leave = new Node(TokenStream.LEAVEWITH);
                        iter.addBeforeCurrent(leave);
                    } else if (elemtype == TokenStream.TRY) {
                        Node jsrFinally = new Node(TokenStream.JSR);
                        Object jsrTarget = n.getProp(Node.FINALLY_PROP);
                        jsrFinally.putProp(Node.TARGET_PROP, jsrTarget);
                        iter.addBeforeCurrent(jsrFinally);
                    } else if (!labelled &&
                               (elemtype == TokenStream.LOOP ||
                                (elemtype == TokenStream.SWITCH &&
                                 type == TokenStream.BREAK)))
                    {
                        /* if it's a simple break/continue, break from the
                         * nearest enclosing loop or switch
                         */
                        loop = n;
                        break;
                    } else if (labelled &&
                               elemtype == TokenStream.LABEL &&
                               id.equals((String)n.getProp(Node.LABEL_PROP)))
                    {
                        loop = n;
                        break;
                    }
                }
                int propType = type == TokenStream.BREAK
                               ? Node.BREAK_PROP
                               : Node.CONTINUE_PROP;
                Node target = loop == null
                              ? null
                              : (Node) loop.getProp(propType);
                if (loop == null || target == null) {
                    String messageId;
                    Object[] messageArgs = null;
                    if (!labelled) {
                        // didn't find an appropriate target
                        if (type == TokenStream.CONTINUE) {
                            messageId = "msg.continue.outside";
                        } else {
                            messageId = "msg.bad.break";
                        }
                    } else if (loop != null) {
                        messageId = "msg.continue.nonloop";
                    } else {
                        messageArgs = new Object[] { id };
                        messageId = "msg.undef.label";
                    }
                    reportError(messageId, messageArgs, node, tree);
                    node.setType(TokenStream.NOP);
                    break;
                }
                node.setType(TokenStream.GOTO);
                node.putProp(Node.TARGET_PROP, target);
                break;
              }

              case TokenStream.CALL:
                if (isSpecialCallName(tree, node))
                    node.putIntProp(Node.SPECIALCALL_PROP, 1);
                visitCall(node, tree);
                break;

              case TokenStream.NEW:
                if (isSpecialCallName(tree, node))
                    node.putIntProp(Node.SPECIALCALL_PROP, 1);
                visitNew(node, tree);
                break;

              case TokenStream.DOT:
              {
                Node right = node.getLastChild();
                right.setType(TokenStream.STRING);
                break;
              }

              case TokenStream.EXPRSTMT:
                node.setType(inFunction ? TokenStream.POP : TokenStream.POPV);
                break;

              case TokenStream.VAR:
              {
                Node result = new Node(TokenStream.BLOCK);
                for (Node cursor = node.getFirstChild(); cursor != null;) {
                    // Move cursor to next before createAssignment get chance
                    // to change n.next
                    Node n = cursor;
                    cursor = cursor.getNext();
                    if (!n.hasChildren())
                        continue;
                    Node init = n.getFirstChild();
                    n.removeChild(init);
                    Node asn = (Node) irFactory.createAssignment(
                                        TokenStream.NOP, n, init, null,
                                        false);
                    Node pop = new Node(TokenStream.POP, asn, node.getLineno());
                    result.addChildToBack(pop);
                }
                iter.replaceCurrent(result);
                break;
              }

              case TokenStream.DELPROP:
              case TokenStream.SETNAME:
              {
                if (!inFunction || inWithStatement())
                    break;
                Node bind = node.getFirstChild();
                if (bind == null || bind.getType() != TokenStream.BINDNAME)
                    break;
                String name = bind.getString();
                Context cx = Context.getCurrentContext();
                if (cx != null && cx.isActivationNeeded(name)) {
                    // use of "arguments" requires an activation object.
                    ((FunctionNode) tree).setRequiresActivation(true);
                }
                if (tree.hasParamOrVar(name)) {
                    if (type == TokenStream.SETNAME) {
                        node.setType(TokenStream.SETVAR);
                        bind.setType(TokenStream.STRING);
                    } else {
                        // Local variables are by definition permanent
                        Node n = new Node(TokenStream.PRIMARY,
                                          TokenStream.FALSE);
                        iter.replaceCurrent(n);
                    }
                }
                break;
              }

              case TokenStream.GETPROP:
                if (inFunction) {
                    Node n = node.getFirstChild().getNext();
                    String name = n == null ? "" : n.getString();
                    Context cx = Context.getCurrentContext();
                    if ((cx != null && cx.isActivationNeeded(name)) ||
                        (name.equals("length") &&
                         cx.getLanguageVersion() == Context.VERSION_1_2))
                    {
                        // Use of "arguments" or "length" in 1.2 requires
                        // an activation object.
                        ((FunctionNode) tree).setRequiresActivation(true);
                    }
                }
                break;

              case TokenStream.NAME:
              {
                if (!inFunction || inWithStatement())
                    break;
                String name = node.getString();
                Context cx = Context.getCurrentContext();
                if (cx != null && cx.isActivationNeeded(name)) {
                    // Use of "arguments" requires an activation object.
                    ((FunctionNode) tree).setRequiresActivation(true);
                }
                if (tree.hasParamOrVar(name)) {
                    node.setType(TokenStream.GETVAR);
                }
                break;
              }
            }
        }
    }

    protected void visitNew(Node node, ScriptOrFnNode tree) {
    }

    protected void visitCall(Node node, ScriptOrFnNode tree) {
        /*
         * For
         *      Call(GetProp(a, b), c, d)   // or GetElem...
         * we wish to evaluate as
         *      Call(GetProp(tmp=a, b), tmp, c, d)
         *
         * for
         *      Call(Name("a"), b, c)
         * we wish to evaluate as
         *      Call(GetProp(tmp=GetBase("a"), "a"), tmp, b, c)
         *
         * and for
         *      Call(a, b, c);
         * we wish to evaluate as
         *      Call(tmp=a, Parent(tmp), c, d)
         */
        Node left = node.getFirstChild();
        // count the arguments
        int argCount = 0;
        Node arg = left.getNext();
        while (arg != null) {
            arg = arg.getNext();
            argCount++;
        }
        boolean addGetThis = false;
        if (left.getType() == TokenStream.NAME) {
            String name = left.getString();
            if (inFunction && tree.hasParamOrVar(name)
                && !inWithStatement())
            {
                // call to a var. Transform to Call(GetVar("a"), b, c)
                left.setType(TokenStream.GETVAR);
                // fall through to code to add GetParent
            } else {
                // transform to Call(GetProp(GetBase("a"), "a"), b, c)

                node.removeChild(left);
                left.setType(TokenStream.GETBASE);
                Node str = left.cloneNode();
                str.setType(TokenStream.STRING);
                Node getProp = new Node(TokenStream.GETPROP, left, str);
                node.addChildToFront(getProp);
                left = getProp;

                // Conditionally set a flag to add a GETTHIS node.
                // The getThis entry in the runtime will take a
                // Scriptable object intended to be used as a 'this'
                // and make sure that it is neither a With object or
                // an activation object.
                // Executing getThis requires at least two instanceof
                // tests, so we only include it if we are currently
                // inside a 'with' statement, or if we are executing
                // a script (to protect against an eval inside a with).
                addGetThis = inWithStatement() || !inFunction;
                // fall through to GETPROP code
            }
        }
        if (left.getType() != TokenStream.GETPROP &&
            left.getType() != TokenStream.GETELEM)
        {
            node.removeChild(left);
            Node tmp = irFactory.createNewTemp(left);
            Node use = irFactory.createUseTemp(tmp);
            use.putProp(Node.TEMP_PROP, tmp);
            Node parent = new Node(TokenStream.PARENT, use);
            node.addChildToFront(parent);
            node.addChildToFront(tmp);
            return;
        }
        Node leftLeft = left.getFirstChild();
        left.removeChild(leftLeft);
        Node tmp = irFactory.createNewTemp(leftLeft);
        left.addChildToFront(tmp);
        Node use = irFactory.createUseTemp(tmp);
        use.putProp(Node.TEMP_PROP, tmp);
        if (addGetThis)
            use = new Node(TokenStream.GETTHIS, use);
        node.addChildAfter(use, left);
    }

    protected boolean inWithStatement() {
        for (int i=loops.size()-1; i >= 0; i--) {
            Node n = (Node) loops.get(i);
            if (n.getType() == TokenStream.WITH)
                return true;
        }
        return false;
    }

    /**
     * Return true if the node is a call to a function that requires
     * access to the enclosing activation object.
     */
    private boolean isSpecialCallName(Node tree, Node node) {
        Node left = node.getFirstChild();
        boolean isSpecial = false;
        if (left.getType() == TokenStream.NAME) {
            String name = left.getString();
            isSpecial = name.equals("eval") || name.equals("With");
        } else {
            if (left.getType() == TokenStream.GETPROP) {
                String name = left.getLastChild().getString();
                isSpecial = name.equals("exec");
            }
        }
        if (isSpecial) {
            // Calls to these functions require activation objects.
            if (inFunction)
                ((FunctionNode) tree).setRequiresActivation(true);
            return true;
        }
        return false;
    }

    private void
    reportError(String messageId, Object[] messageArgs, Node stmt,
                ScriptOrFnNode tree)
    {
        int lineno = stmt.getLineno();
        String sourceName = tree.getSourceName();
        irFactory.ts.reportSyntaxError(true, messageId, messageArgs,
                                       sourceName, lineno, null, 0);
    }

    private ObjArray loops;
    private ObjArray loopEnds;
    private boolean inFunction;
    protected IRFactory irFactory;
}

