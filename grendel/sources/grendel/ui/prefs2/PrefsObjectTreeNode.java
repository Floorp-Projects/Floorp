/*
 * AddressBookValueTreeNode.java
 *
 * Created on 08 October 2005, 18:52
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.ui.prefs2;

import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;


/**
 *
 * @author hash9
 */
public class PrefsObjectTreeNode extends DefaultMutableTreeNode
        implements TreeNode {
    private Object value;
    
    /**
     * Creates a new instance of PrefsObjectTreeNode
     */
    public PrefsObjectTreeNode(MutableTreeNode parent, String key, Object o) {
        super(key);
        super.parent = parent;
        this.value = o;
    }
    
    public Object getValue() {
        return value;
    }
    
    public boolean equals(Object obj) {
        if (obj instanceof PrefsObjectTreeNode) {
            PrefsObjectTreeNode n = (PrefsObjectTreeNode) obj;
            if (n.parent == parent) {
                if (n.userObject.equals(userObject)) {
                    return true;
                }
            }
        }
        return false;
    }
    
}
