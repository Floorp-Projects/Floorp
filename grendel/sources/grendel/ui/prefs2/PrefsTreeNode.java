/*
 * AddressBookValueTreeNode.java
 *
 * Created on 08 October 2005, 18:52
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.ui.prefs2;

import grendel.addressbook.AddressBookFeildValue;
import grendel.prefs.xml.XMLPreferences;
import grendel.widgets.TreeTableModelListener;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.Vector;
import javax.swing.event.TreeExpansionEvent;
import javax.swing.event.TreeModelEvent;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;

/**
 *
 * @author hash9
 */
public class PrefsTreeNode extends DefaultMutableTreeNode implements TreeNode {
    private XMLPreferences prefs;
    
    /**
     * Creates a new instance of PrefsTreeNode
     */
    public PrefsTreeNode(MutableTreeNode parent, String key, XMLPreferences prefs) {
        super(key);
        super.parent = parent;
        this.prefs = prefs;
    }
    
    public int getChildCount() {
        return prefs.size();
    }
    
    
    public boolean getAllowsChildren() {
        return true;
    }
    
    public boolean isLeaf() {
        return false;
    }
    
    public int getIndex(TreeNode aChild) {
        return -1;
    }
    
    public TreeNode getChildAt(int childIndex) {
        String key = (String) prefs.keySet().toArray()[childIndex];
        Object o = prefs.getProperty(key);
        if (o instanceof XMLPreferences) {
            return new PrefsTreeNode(this,key, (XMLPreferences) o);
        } else {
            return new PrefsObjectTreeNode(this, key, o);
        }
    }

    public XMLPreferences getPrefs() {
        return prefs;
    }
}
