/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s):  Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.util.Enumeration;
import javax.swing.tree.TreeNode;
import javax.swing.tree.MutableTreeNode;

public abstract class RDFTreeNode extends ISupportsPeer implements MutableTreeNode
{
//
// Constants
//

//
// Class Variables
//

//
// Instance Variables
//

// Attribute Instance Variables

// Relationship Instance Variables

private int nativeRDFNode = -1;

private RDFTreeNode parent;


/** 
      
 * a handle to the actual mozilla webShell, owned, allocated, and
 * released by WindowControl
   
 */
  
public int nativeWebShell = -1;


//
// Constructors and Initializers    
//

protected RDFTreeNode(int yourNativeWebShell, 
                      int nativeNode, RDFTreeNode yourParent)
{
    nativeWebShell = yourNativeWebShell;
    nativeRDFNode = nativeNode;
    parent = yourParent;
}


//
// Class methods
//

//
// General Methods
//

//
// Abstract Methods
//

protected abstract RDFTreeNode newRDFTreeNode(int nativeWebShell, 
                                              int nativeNode, 
                                              RDFTreeNode yourParent);

int getNativeRDFNode()
{
    return nativeRDFNode;
}

private void setNativeRDFNode(int yourNativeRDFNode)
{
    if (-1 != nativeRDFNode) {
        throw new IllegalStateException("Can't call setNativeRDFNode() more than once on an instance.");
    }
    nativeRDFNode = yourNativeRDFNode;
}

//
// Methods from Object
//

protected void finalize() throws Throwable
{
    nativeRelease(nativeRDFNode);
    super.finalize();
}

public String toString()
{
    String result = null;
    if (-1 != nativeRDFNode) {
      result = nativeToString(nativeWebShell, nativeRDFNode);
    }
    else {
        result = super.toString();
    }
    return result;
}


//
// Methods from TreeNode
//

public Enumeration children() 
{
    Assert.assert(-1 != nativeRDFNode);
    Enumeration enum = null;

    enum = new RDFEnumeration(nativeWebShell, this);

    return enum;
}

public boolean getAllowsChildren()
{
    boolean result = true;

    return result;
}
 
public TreeNode getChildAt(int childIndex)
{
    Assert.assert(-1 != nativeRDFNode);
    TreeNode result = null;
    int childNode;

    if (!isLeaf()) {
      if (-1 != (childNode = nativeGetChildAt(nativeWebShell, nativeRDFNode, 
					      childIndex))) {
	result = newRDFTreeNode(nativeWebShell, childNode, this);
      }
    }
    
    return result;
}
 
public int getChildCount()
{
    Assert.assert(-1 != nativeRDFNode);
    int result = -1;

    result = nativeGetChildCount(nativeWebShell, nativeRDFNode);

    return result;
}

public int getIndex(TreeNode node)
{
    Assert.assert(-1 != nativeRDFNode);
    int result = -1;
    if (node instanceof RDFTreeNode) {
      result = nativeGetIndex(nativeWebShell, nativeRDFNode, 
			      ((RDFTreeNode)node).nativeRDFNode);
    }

    return result;
}

public TreeNode getParent() 
{
    Assert.assert(-1 != nativeRDFNode);
    return parent;
}

public boolean isLeaf()
{
    Assert.assert(-1 != nativeRDFNode);

    return nativeIsLeaf(nativeWebShell, nativeRDFNode);
}
    

// 
// Methods from MutableTreeNode
//

public void insert(MutableTreeNode child, int index)
{
    if (!(child instanceof RDFTreeNode)) {
        throw new IllegalArgumentException("Can't insert non-RDFTreeNode children");
    }
    Assert.assert(-1 != nativeRDFNode);
    RDFTreeNode childNode = (RDFTreeNode) child;
    Assert.assert(-1 != childNode.getNativeRDFNode());
    int childNativeRDFNode = childNode.getNativeRDFNode();

    // hook up the child to its native peer
    nativeInsertElementAt(nativeWebShell, nativeRDFNode, 
			  childNativeRDFNode, index);

    // hook up the child to its java parent
    childNode.setParent(this);
    
}

public void remove(int index) 
{

}

public void remove(MutableTreeNode node) 
{

}

public void removeFromParent() 
{

}

public void setParent(MutableTreeNode newParent) 
{
    if (newParent instanceof RDFTreeNode) {
        parent = (RDFTreeNode) newParent;
    }
}

public void setUserObject(Object object) 
{

}

//
// Native methods
//

public native boolean nativeIsLeaf(int webShellPtr, int nativeRDFNode);
public native boolean nativeIsContainer(int webShellPtr, int nativeRDFNode);

/**

 * the returned child has already been XPCOM AddRef'd

 */

public native int nativeGetChildAt(int webShellPtr, int nativeRDFNode, 
                                   int childIndex);
public native int nativeGetChildCount(int webShellPtr, int nativeRDFNode);
public native int nativeGetIndex(int webShellPtr, int nativeRDFNode, 
                                 int childRDFNode);
public native String nativeToString(int webShellPtr, int nativeRDFNode);
public native void nativeInsertElementAt(int webShellPtr, 
                                         int parentNativeRDFNode,
                                         int childNativeRDFNode, int index);

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("RDFTreeNode");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: RDFTreeNode.java,v 1.2 2000/11/03 03:16:50 edburns%acm.org Exp $");

}

// ----VERTIGO_TEST_END

} // end of class RDFTreeNode
