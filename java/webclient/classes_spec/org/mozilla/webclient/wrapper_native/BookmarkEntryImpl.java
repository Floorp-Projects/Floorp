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

import org.mozilla.webclient.Bookmarks;
import org.mozilla.webclient.BookmarkEntry;
import org.mozilla.webclient.UnimplementedException;

import java.util.Properties;

import javax.swing.tree.MutableTreeNode;

public class BookmarkEntryImpl extends RDFTreeNode implements BookmarkEntry
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

Properties properties = null;


//
// Constructors and Initializers    
//

protected BookmarkEntryImpl(int nativeNode, RDFTreeNode yourParent)
{
    super(nativeNode, yourParent);
}

//
// Class methods
//

//
// General Methods
//

//
// Abstract Method Implementations
//

protected RDFTreeNode newRDFTreeNode(int nativeNode, 
                                     RDFTreeNode yourParent)
{
    return new BookmarkEntryImpl(nativeNode, yourParent);
}

//
// Methods from javax.swing.tree.MutableTreeNode
//

public void remove(int index) 
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function BookmarkEntry::remove has not yet been implemented.\n");
}

public void remove(MutableTreeNode node) 
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function BookmarkEntry::remove has not yet been implemented.\n");
}

public void removeFromParent() 
{
    throw new UnimplementedException("\nUnimplementedException -----\n API Function BookmarkEntry::removeFromParent has not yet been implemented.\n");
}

public void setUserObject(Object object) 
{

}


//
// Methods from BookmarkEntry    
//

public Properties getProperties()
{
    if ((null == properties) &&
        (null == (properties = new Properties()))) {
        throw new IllegalStateException("Can't create properties table");
    }
    
    return properties;
}

public boolean isFolder()
{
    return nativeIsContainer(getNativeRDFNode());
}

// ----VERTIGO_TEST_START

//
// Test methods
//

public static void main(String [] args)
{
    Assert.setEnabled(true);

    Log.setApplicationName("BookmarkEntryImpl");
    Log.setApplicationVersion("0.0");
    Log.setApplicationVersionDate("$Id: BookmarkEntryImpl.java,v 1.2 2000/08/04 21:46:10 edburns%acm.org Exp $");

}

// ----VERTIGO_TEST_END

} // end of class BookmarksImpl
