/*
 * AddressBookValueTreeNode.java
 *
 * Created on 08 October 2005, 18:52
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.ui.addressbook2;

import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressBookFeildValueList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.Vector;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.MutableTreeNode;
import javax.swing.tree.TreeNode;

/**
 *
 * @author hash9
 */
public class AddressBookFeildValueTreeNode extends DefaultMutableTreeNode implements TreeNode
{
  
  /**
   * Creates a new instance of AddressBookFeildValueTreeNode
   */
  public AddressBookFeildValueTreeNode(MutableTreeNode parent, String key, AddressBookFeildValueList values)
  {
    super(key);
    super.parent = parent;
    children = values;
  }

  public boolean getAllowsChildren()
  {
    return true;
  }

  public boolean isLeaf()
  {
    return false;
  }

  public int getIndex(TreeNode aChild)
  {
    return -1;
  }
  
  public TreeNode getChildAt(int childIndex)
  {
    try {
    TreeNode o = new DefaultMutableTreeNode(children.get(childIndex));
    return o;
    } catch (Exception e) {
      e.printStackTrace();
      return null;
    }
    
  }
}
