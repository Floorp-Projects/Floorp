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
    private Hashtable theFnClassNameList;

    OptTransformer(IRFactory irFactory, Hashtable theFnClassNameList) {
        super(irFactory);
        this.theFnClassNameList = theFnClassNameList;
    }

    public NodeTransformer newInstance() {
        Hashtable listCopy = (Hashtable) theFnClassNameList.clone();
        return new OptTransformer(irFactory, listCopy);
    }

    public ScriptOrFnNode transform(ScriptOrFnNode scriptOrFn) {

        // Collect all of the script contained functions into a hashtable
        // so that the call optimizer can access the class name & parameter
        // count for any call it encounters
        if (scriptOrFn.getType() == TokenStream.SCRIPT) {
            collectContainedFunctions(scriptOrFn);
        }
        return super.transform(scriptOrFn);
    }

    private int detectDirectCall(Node node, ScriptOrFnNode tree)
    {
        Context cx = Context.getCurrentContext();
        int optLevel = cx.getOptimizationLevel();
        Node left = node.getFirstChild();

        // count the arguments
        int argCount = 0;
        Node arg = left.getNext();
        while (arg != null) {
            arg = arg.getNext();
            argCount++;
        }

        if (tree.getType() == TokenStream.FUNCTION && optLevel > 0) {
            if (left.getType() == TokenStream.NAME) {
                markDirectCall(tree, node, argCount, left.getString());
            } else {
                if (left.getType() == TokenStream.GETPROP) {
                    Node name = left.getFirstChild().getNext();
                    markDirectCall(tree, node, argCount, name.getString());
                }
            }
        }

        return argCount;
    }

    protected void visitNew(Node node, ScriptOrFnNode tree) {
        detectDirectCall(node, tree);
        super.visitNew(node, tree);
    }

    protected void visitCall(Node node, ScriptOrFnNode tree) {
        int argCount = detectDirectCall(node, tree);
        if (inFunction && (argCount == 0)) {
            ((OptFunctionNode)tree).itsContainsCalls0 = true;
        }

        super.visitCall(node, tree);
    }

    /*
     * Optimize a call site by converting call("a", b, c) into :
     *
     *       FunctionObjectFor"a" <-- instance variable init'd by constructor
     *
     *       // this is a DIRECTCALL node
     *       fn = GetProp(tmp = GetBase("a"), "a");
     *       if (fn == FunctionObjectFor"a")
     *           fn.call(tmp, b, c)
     *       else
     *           ScriptRuntime.Call(fn, tmp, b, c)
     */
    private void markDirectCall(Node containingTree, Node callNode,
                                int argCount, String targetName)
    {
        OptFunctionNode theFunction
                    = (OptFunctionNode)theFnClassNameList.get(targetName);
        if (theFunction != null) {
            int N = theFunction.getParamCount();
            // Refuse to directCall any function with more
            // than 32 parameters - prevent code explosion
            // for wacky test cases
            if (N > 32)
                return;

            if (argCount == N) {
                callNode.putProp(Node.DIRECTCALL_PROP, theFunction);
                theFunction.setIsTargetOfDirectCall();
            }
        }
    }

    /**
     * Collect all of the contained functions into a hashtable
     * so that the call optimizer can access the class name & parameter
     * count for any call it encounters
     */
    private void collectContainedFunctions(ScriptOrFnNode scriptOrFn) {
        int functionCount = scriptOrFn.getFunctionCount();
        for (int i = 0; i != functionCount; ++i) {
            OptFunctionNode f = (OptFunctionNode)scriptOrFn.getFunctionNode(i);
            if (f.getType() == FunctionNode.FUNCTION_STATEMENT) {
                String name = f.getFunctionName();
                if (name.length() != 0) {
                    theFnClassNameList.put(name, f);
                }
            }
        }
    }

}
