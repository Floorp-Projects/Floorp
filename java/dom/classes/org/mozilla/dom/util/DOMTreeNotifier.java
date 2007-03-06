/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 *
 * Contributor(s): Igor Kushnirskiy <idk@eng.sun.com>
 */

package org.mozilla.dom.util;

import javax.swing.event.TreeModelEvent;

public interface DOMTreeNotifier {
    /*
     * Invoked after a node (or a set of siblings) has changed in some way.
     */
    public void treeNodesChanged(TreeModelEvent e);
                
    /*
     * Invoked after nodes have been inserted into the tree.
     */
    public void treeNodesInserted(TreeModelEvent e);
    
    /*
     * Invoked after nodes have been removed from the tree.
     */
    public void treeNodesRemoved(TreeModelEvent e);
    /*
     * Invoked after the tree has drastically changed structure from a given node down.
     */
    public void treeStructureChanged(TreeModelEvent e);
};

