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
import grendel.addressbook.AddressCard;
import java.util.Enumeration;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.TreeNode;

/**
 *
 * @author hash9
 */
public class AddressBookCardTreeNode extends DefaultMutableTreeNode implements TreeNode
{
  private AddressCard card;
  /**
   * Creates a new instance of AddressBookCardTreeNode
   */
  public AddressBookCardTreeNode(AddressCard card)
  {
    super(card);
    this.card = card;
  }
  
  public TreeNode getChildAt(int childIndex)
  {
    String key = (String) card.getAllValues().keySet().toArray()[childIndex];
    AddressBookFeildValueList values = card.getValues(key);
    return new AddressBookFeildValueTreeNode(this,key,values);
  }
  
  public int getChildCount()
  {
    return card.getAllValues().keySet().size();
  }
  
  public int getIndex(TreeNode aChild)
  {
    return -1;
  }
  
  public TreeNode getParent()
  {
    return null;
  }
  
  public boolean getAllowsChildren()
  {
    return true;
  }
  
  public Enumeration children()
  {
    return null;
  }
  
}
