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

package org.mozilla.webclient.test;

import javax.swing.tree.TreeCellRenderer;
import javax.swing.JTree;

import org.w3c.dom.Node;

import java.awt.Component;

class DOMCellRenderer implements TreeCellRenderer {
    private TreeCellRenderer cellRenderer;
    public DOMCellRenderer(TreeCellRenderer cellRenderer) {
	this.cellRenderer = cellRenderer;
    }
    	
    public Component getTreeCellRendererComponent(JTree tree, Object value, 
						  boolean selected,
						  boolean expanded, 
						  boolean leaf, int row,
						  boolean hasFocus) {
	return cellRenderer.getTreeCellRendererComponent(tree, 
							 ((Node)value).getNodeName(),
							 selected,
							 expanded, 
							 leaf,  row, 
							 hasFocus);
    }

}
