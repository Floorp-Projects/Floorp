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

public class RDFEnumeration extends Object implements Enumeration
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

 * set from native code

 */

private int nativeEnum = -1;

/**

 * set from native code

 */

private int nativeContainer = -1;

/** 
      
 * a handle to the actual mozilla webShell, owned, allocated, and
 * released by WindowControl
   
 */
  
public int nativeWebShell = -1;



//
// Constructors and Initializers    
//

public RDFEnumeration(int yourNativeWebShell, RDFTreeNode enumParent)
{
    nativeWebShell = yourNativeWebShell;
    parent = enumParent;
    nativeRDFNode = parent.getNativeRDFNode();
}

//
// Class methods
//

//
// General Methods
//

//
// Methods from Object
//

protected void finalize() throws Throwable
{
  nativeFinalize(nativeWebShell);
    super.finalize();
}

//
// Methods from Enumeration
//

public boolean hasMoreElements()
{
    Assert.assert(-1 != nativeRDFNode);
    return nativeHasMoreElements(nativeWebShell, nativeRDFNode);
}

public Object nextElement()
{
    Assert.assert(null != parent);
    Object result = null;
    int nextNativeRDFNode;

    if (-1 != (nextNativeRDFNode = nativeNextElement(nativeWebShell,
						     nativeRDFNode))) {
      result = parent.newRDFTreeNode(nativeWebShell,
				     nextNativeRDFNode, parent);
    }

    return result;
}

// 
// Native Methods
// 

private native boolean nativeHasMoreElements(int webShellPtr, 
                                             int nativeRDFNode);

/**

 * @return -1 if no more elements, a pointer otherwise.

 */ 

private native int nativeNextElement(int webShellPtr, 
                                     int nativeRDFNode);
protected native void nativeFinalize(int webShellPtr);

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("RDFEnumeration");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: RDFEnumeration.java,v 1.2 2000/11/03 03:16:50 edburns%acm.org Exp $");

}

// ----VERTIGO_TEST_END

} // end of class RDFEnumeration
