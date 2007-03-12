/* 
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
import org.mozilla.util.ReturnRunnable;

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
      
 * <p>the nativeContext, owned by WrapperFactory</p>
 * 
   
 */
  
public int nativeContext = -1;



//
// Constructors and Initializers    
//

public RDFEnumeration(int yourNativeContext, 
                      RDFTreeNode enumParent)
{
    nativeContext = yourNativeContext;
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
    NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
	    public Object run() {
		nativeFinalize(RDFEnumeration.this.nativeContext);
		return null;
	    }
            public String toString() {
                return "WCRunnable.nativeFinalize";
            }
            
	});
    super.finalize();
}

//
// Methods from Enumeration
//

public boolean hasMoreElements()
{
    Assert.assert_it(-1 != nativeRDFNode);
    Boolean result = (Boolean)
	NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    Boolean result = 
			new Boolean(nativeHasMoreElements(RDFEnumeration.this.nativeContext, 
							  RDFEnumeration.this.nativeRDFNode));
		    return result;
		}
                public String toString() {
                    return "WCRunnable.nativeHasMoreElements";
                }

	    });
    return result.booleanValue();
}

public Object nextElement()
{
    Assert.assert_it(null != parent);
    Object result = null;
    Integer nextNativeRDFNode = (Integer)
	NativeEventThread.instance.pushBlockingReturnRunnable(new ReturnRunnable() {
		public Object run() {
		    Integer result = 
			new Integer(nativeNextElement(RDFEnumeration.this.nativeContext,
						      RDFEnumeration.this.nativeRDFNode));
		    return result;
		}
                public String toString() {
                    return "WCRunnable.nativeNextElement";
                }

	    });
    
    if (-1 != nextNativeRDFNode.intValue()) {
	result = parent.newRDFTreeNode(nativeContext,
				       nextNativeRDFNode.intValue(), parent);
    }

    return result;
}

// 
// Native Methods
// 

private native boolean nativeHasMoreElements(int nativeContext, 
                                             int nativeRDFNode);

/**

 * @return -1 if no more elements, a pointer otherwise.

 */ 

private native int nativeNextElement(int nativeContext, 
                                     int nativeRDFNode);
protected native void nativeFinalize(int nativeContext);


} // end of class RDFEnumeration
