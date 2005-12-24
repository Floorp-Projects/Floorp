/*
 * AddressCardTreeTable.java
 *
 * Created on 08 October 2005, 17:26
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.ui.addressbook2;

import grendel.addressbook.AddressBookFeildValue;
import grendel.addressbook.AddressCard;
import javax.swing.tree.DefaultMutableTreeNode;
import org.jdesktop.swing.treetable.DefaultTreeTableModel;
import org.jdesktop.swing.treetable.TreeTableModel;
/**
 *
 * @author hash9
 */
public class AddressCardTreeTableModel extends DefaultTreeTableModel implements TreeTableModel
{
  private AddressCard card;
  /**
   * Creates a new instance of AddressCardTreeTableModel
   */
  public AddressCardTreeTableModel(AddressCard card)
  {
    super(new AddressBookCardTreeNode(card));
    this.card =card;
  }
  
  public Object getValueAt(Object object, int i)
  {
    if (object instanceof DefaultMutableTreeNode)
    {
      DefaultMutableTreeNode n =(DefaultMutableTreeNode) object;
      if (n.getUserObject() instanceof AddressBookFeildValue)
      {
        AddressBookFeildValue value = (AddressBookFeildValue) n.getUserObject();
        if (i==0)
        {
          return "";
        }
        else if (i==1)
        {
          return Boolean.toString(value.isSystem());
        }
        else if (i==2)
        {
          return Boolean.toString(value.isHidden());
        }
        else if (i==3)
        {
          return Boolean.toString(value.isReadonly());
        }
        else
        {
          return "";
        }
      }
    }
    return "";
  }
  
  public int getColumnCount()
  {
    return 4;
  }
  
  public String getColumnName(int i)
  {
    switch(i)
    {
      case 0:
        return "field";
      case 1:
        return "system";
      case 2:
        return "hidden";
      case 3:
        return "read only";
      default:
        return "";
    }
  }
  
  /*public Object getChild(Object parent, int index)
  {
    if (parent instanceof AddressCard)
    {
      return card.getAllValues().keySet().toArray()[index];
    }
    else if (parent instanceof String)
    {
      return card.getValues((String)parent)[index];
    }
    else
    {
      return null;
    }
  }/*
   
  /*public int getChildCount(Object parent)
  {
    if (parent instanceof AddressCard)
    {
      return card.getAllValues().keySet().size();
    }
    else if (parent instanceof String)
    {
      return card.getValues((String)parent).length;
    }
    else
    {
      return -1;
    }
  }*/
}
