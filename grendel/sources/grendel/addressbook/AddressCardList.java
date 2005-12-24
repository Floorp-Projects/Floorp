/*
 * AddressList.java
 *
 * Created on 23 September 2005, 22:13
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook;

import grendel.storage.addressparser.AddressList;
import java.util.List;
import java.util.Vector;

/**
 * A List of Address Cards. These may have come from one or more address books.
 * The list may represent any union of any subset of an addressbooks.
 * @author Kieran Maclean
 */
public abstract class AddressCardList extends Vector<AddressCard> implements List<AddressCard>
{
 
  protected AddressCardList()
  {
  }
  
  /**
   * @return a <code>AddressList</code> sutible for use in sending mail
   */
  public abstract AddressList getAddressList();
  
}
