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

import org.mozilla.webclient.Bookmarks;
import org.mozilla.webclient.BookmarkEntry;
import org.mozilla.webclient.UnimplementedException;

import javax.swing.tree.MutableTreeNode;
import java.util.Properties;

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

//
// Constructors and Initializers    
//

protected BookmarkEntryImpl(int nativeContext, int nativeNode, 
                            RDFTreeNode yourParent)
{
    super(nativeContext, nativeNode, yourParent);
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

protected RDFTreeNode newRDFTreeNode(int nativeContext, int nativeNode, 
                                     RDFTreeNode yourParent)
{
    return new BookmarkEntryImpl(nativeContext, nativeNode, yourParent);
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
    return super.getProperties();
}

public boolean isFolder()
{
    return super.isFolder();
}

} // end of class BookmarksImpl
