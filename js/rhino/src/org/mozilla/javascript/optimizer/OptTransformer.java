/*
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


package org.mozilla.javascript.optimizer;

import org.mozilla.javascript.*;
import java.util.Hashtable;

/**
 * This class performs node transforms to prepare for optimization.
 *
 * @see NodeTransformer
 * @author Norris Boyd
 */

class OptTransformer extends NodeTransformer {

    OptTransformer(CompilerEnvirons compilerEnv, Hashtable possibleDirectCalls,
                   ObjArray directCallTargets)
    {
        super(compilerEnv);
        this.possibleDirectCalls = possibleDirectCalls;
        this.directCallTargets = directCallTargets;
    }

    protected void visitNew(Node node, ScriptOrFnNode tree) {
        detectDirectCall(node, tree);
        super.visitNew(node, tree);
    }

    protected void visitCall(Node node, ScriptOrFnNode tree) {
        detectDirectCall(node, tree);

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
        boolean addGetThis = false;
        if (left.getType() == Token.NAME) {
            String name = left.getString();
            boolean inFunction = (tree.getType() == Token.FUNCTION);
            if (inFunction && tree.hasParamOrVar(name)
                && !inWithStatement())
            {
                // call to a var. Transform to Call(GetVar("a"), b, c)
                left.setType(Token.GETVAR);
                // fall through to code to add GetParent
            } else {
                // transform to Call(GetProp(GetBase("a"), "a"), b, c)

                node.removeChild(left);
                left.setType(Token.GETBASE);
                Node str = Node.newString(left.getString());
                Node getProp = new Node(Token.GETPROP, left, str);
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
        if (left.getType() != Token.GETPROP &&
            left.getType() != Token.GETELEM)
        {
            node.removeChild(left);
            Node tmp = createNewTemp(left);
            Node use = createUseTemp(tmp);
            use.putProp(Node.TEMP_PROP, tmp);
            Node parent = new Node(Token.PARENT, use);
            node.addChildToFront(parent);
            node.addChildToFront(tmp);
            return;
        }
        Node leftLeft = left.getFirstChild();
        left.removeChild(leftLeft);
        Node tmp = createNewTemp(leftLeft);
        left.addChildToFront(tmp);
        Node use = createUseTemp(tmp);
        use.putProp(Node.TEMP_PROP, tmp);
        if (addGetThis)
            use = new Node(Token.GETTHIS, use);
        node.addChildAfter(use, left);

        super.visitCall(node, tree);
    }

    private void detectDirectCall(Node node, ScriptOrFnNode tree)
    {
        if (tree.getType() == Token.FUNCTION) {
            Node left = node.getFirstChild();

            // count the arguments
            int argCount = 0;
            Node arg = left.getNext();
            while (arg != null) {
                arg = arg.getNext();
                argCount++;
            }

            if (argCount == 0) {
                OptFunctionNode.get(tree).itsContainsCalls0 = true;
            }

            /*
             * Optimize a call site by converting call("a", b, c) into :
             *
             *  FunctionObjectFor"a" <-- instance variable init'd by constructor
             *
             *  // this is a DIRECTCALL node
             *  fn = GetProp(tmp = GetBase("a"), "a");
             *  if (fn == FunctionObjectFor"a")
             *      fn.call(tmp, b, c)
             *  else
             *      ScriptRuntime.Call(fn, tmp, b, c)
             */
            if (possibleDirectCalls != null) {
                String targetName = null;
                if (left.getType() == Token.NAME) {
                    targetName = left.getString();
                } else if (left.getType() == Token.GETPROP) {
                    targetName = left.getFirstChild().getNext().getString();
                }
                if (targetName != null) {
                    OptFunctionNode ofn;
                    ofn = (OptFunctionNode)possibleDirectCalls.get(targetName);
                    if (ofn != null
                        && argCount == ofn.fnode.getParamCount()
                        && !ofn.fnode.requiresActivation())
                    {
                        // Refuse to directCall any function with more
                        // than 32 parameters - prevent code explosion
                        // for wacky test cases
                        if (argCount <= 32) {
                            node.putProp(Node.DIRECTCALL_PROP, ofn);
                            if (!ofn.isTargetOfDirectCall()) {
                                int index = directCallTargets.size();
                                directCallTargets.add(ofn);
                                ofn.setDirectTargetIndex(index);
                            }
                        }
                    }
                }
            }
        }
    }

    private static Node createNewTemp(Node n) {
        int type = n.getType();
        if (type == Token.STRING || type == Token.NUMBER) {
            // Optimization: clone these values rather than storing
            // and loading from a temp
            return n;
        }
        return new Node(Token.NEWTEMP, n);
    }

    private static Node createUseTemp(Node newTemp)
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
            throw Kit.codeBug();
        }
    }

    private Hashtable possibleDirectCalls;
    private ObjArray directCallTargets;
}
