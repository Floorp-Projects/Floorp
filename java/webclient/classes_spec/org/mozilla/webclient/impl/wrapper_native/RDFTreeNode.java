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

package org.mozilla.webclient.impl.wrapper_native;

import org.mozilla.util.Assert;
import org.mozilla.util.Log;
import org.mozilla.util.ParameterCheck;

import java.util.Enumeration;
import java.util.Properties;

import javax.swing.tree.TreeNode;
import javax.swing.tree.MutableTreeNode;

import org.mozilla.webclient.BookmarkEntry;
import org.mozilla.webclient.UnimplementedException;

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

protected int nativeRDFNode = -1;

private RDFTreeNode parent;

Properties properties = null;

/** 
 * the native pointer from WrapperFactory.
 */
  
public int nativeContext = -1;


//
// Constructors and Initializers    
//

protected RDFTreeNode(int yourNativeWebShell, 
                      int nativeNode, RDFTreeNode yourParent)
{
    nativeContext = yourNativeWebShell;
    nativeRDFNode = nativeNode;
    parent = yourParent;
}


//
// Class methods
//

//
// General Methods
//

public Properties getProperties()
{
    if ((null == properties) &&
        (null == (properties = new Properties()))) {
        throw new IllegalStateException("Can't create properties table");
    }
    
    return properties;
}

//
// Abstract Methods
//

protected abstract RDFTreeNode newRDFTreeNode(int nativeContext, 
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
    if (-1 != nativeRDFNode) {
        nativeRelease(nativeRDFNode);
    }
    super.finalize();
}

public String toString()
{
    String result = null;
    if (-1 != nativeRDFNode) {
      result = nativeToString(nativeContext, nativeRDFNode);
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
    Assert.assert_it(-1 != nativeRDFNode);
    Enumeration enumer = null;

    enumer = new RDFEnumeration(nativeContext, this);

    return enumer;
}

public boolean getAllowsChildren()
{
    throw new UnimplementedException("\nUnimplementedException-----\n API Function RDFTreeNode.getAllowsChildren() has not yet been implemented.\n");
}
 
public TreeNode getChildAt(int childIndex)
{
    Assert.assert_it(-1 != nativeRDFNode);
    TreeNode result = null;
    int childNode;

    if (!isLeaf()) {
      if (-1 != (childNode = nativeGetChildAt(nativeContext, nativeRDFNode, 
					      childIndex))) {
	result = newRDFTreeNode(nativeContext, childNode, this);
      }
    }
    
    return result;
}
 
public int getChildCount()
{
    Assert.assert_it(-1 != nativeRDFNode);
    int result = -1;

    result = nativeGetChildCount(nativeContext, nativeRDFNode);

    return result;
}

public int getIndex(TreeNode node)
{
    Assert.assert_it(-1 != nativeRDFNode);
    int result = -1;
    if (node instanceof RDFTreeNode) {
      result = nativeGetIndex(nativeContext, nativeRDFNode, 
			      ((RDFTreeNode)node).nativeRDFNode);
    }

    return result;
}

public TreeNode getParent() 
{
    Assert.assert_it(-1 != nativeRDFNode);
    return parent;
}

public boolean isLeaf()
{
    Assert.assert_it(-1 != nativeRDFNode);

    return nativeIsLeaf(nativeContext, nativeRDFNode);
}

// 
// Methods from MutableTreeNode
//

/**
 
 * Unfortunately, we have to handle folders and bookmarks differently.

 */

public void insert(MutableTreeNode child, int index)
{
    if (!(child instanceof RDFTreeNode)) {
        throw new IllegalArgumentException("Can't insert non-RDFTreeNode children");
    }
    Assert.assert_it(-1 != nativeRDFNode);
    RDFTreeNode childNode = (RDFTreeNode) child;
    
    if (childNode.isFolder()) {
        Assert.assert_it(-1 == childNode.getNativeRDFNode());
        Assert.assert_it(null != childNode.getProperties());
        int childNativeRDFNode;
        
        // hook up the child to its native peer
        childNativeRDFNode = nativeNewFolder(nativeContext, nativeRDFNode, 
                                             childNode.getProperties());
    
        // hook up the child to its native peer
        childNode.setNativeRDFNode(childNativeRDFNode);
    }
    else {
        Assert.assert_it(-1 != childNode.getNativeRDFNode());
        int childNativeRDFNode = childNode.getNativeRDFNode();
        
        // hook up the child to its native peer
        nativeInsertElementAt(nativeContext, nativeRDFNode, 
                              childNativeRDFNode, childNode.getProperties(), 
                              index);
    }
        
    // hook up the child to its java parent
    childNode.setParent(this);
    
}

public void remove(int index) 
{
    throw new UnimplementedException("\nUnimplementedException-----\n API Function RDFTreeNode.remove(int) has not yet been implemented.\n");
}

public void remove(MutableTreeNode node) 
{
    throw new UnimplementedException("\nUnimplementedException-----\n API Function RDFTreeNode.remove(MutableTreeNode) has not yet been implemented.\n");
}

public void removeFromParent() 
{
    throw new UnimplementedException("\nUnimplementedException-----\n API Function RDFTreeNode.removeFromParent() has not yet been implemented.\n");
}

public void setParent(MutableTreeNode newParent) 
{
    if (newParent instanceof RDFTreeNode) {
        parent = (RDFTreeNode) newParent;
    }
}

public void setUserObject(Object object) 
{
    throw new UnimplementedException("\nUnimplementedException-----\n API Function RDFTreeNode.setUserObject(Object) has not yet been implemented.\n");
}

//
// methods on this
//
public boolean isFolder()
{
    boolean result = false;
    if (-1 == nativeRDFNode) {
        if (null != getProperties()) {
            result = (null != getProperties().get(BookmarkEntry.IS_FOLDER));
        }
    }
    else {
        result = nativeIsContainer(nativeContext, getNativeRDFNode());
    }
    return result;
}


//
// Native methods
//

public native boolean nativeIsLeaf(int nativeContext, int nativeRDFNode);
public native boolean nativeIsContainer(int nativeContext, int nativeRDFNode);

/**

 * the returned child has already been XPCOM AddRef'd

 */

public native int nativeGetChildAt(int nativeContext, int nativeRDFNode, 
                                   int childIndex);
public native int nativeGetChildCount(int nativeContext, int nativeRDFNode);
public native int nativeGetIndex(int nativeContext, int nativeRDFNode, 
                                 int childRDFNode);
public native String nativeToString(int nativeContext, int nativeRDFNode);
public native void nativeInsertElementAt(int nativeContext, 
                                         int parentNativeRDFNode,
                                         int childNativeRDFNode, 
                                         Properties childProps, int index);
public native int nativeNewFolder(int nativeContext, int parentRDFNode, 
                                  Properties childProps);

} // end of class RDFTreeNode
