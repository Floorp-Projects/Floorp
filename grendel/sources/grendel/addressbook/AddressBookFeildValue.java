/*
 * AddressBookFeildValue.java
 *
 * Created on 30 September 2005, 14:48
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel.addressbook;


/**
 * A perticular address card feild value.
 * This is a singe value for a single feild in an address book.
 * @author Kieran Maclean
 */
public class AddressBookFeildValue
{
  /**
   * The value.
   */
  private String value = null;

  /**
   * Is the value hidden. Should the user be allowed to see this value.
   */
  private boolean hidden = false;

  /**
   * Is the value read only. Should the user be permitted to change this value.
   */
  private boolean readonly = false;

  /**
   * Is the value a system value. That is a special one that the user probably doesn't nead to see.
   */
  private boolean system = false;

  /**
     * Creates a new <code>AddressBookFeildValue</code> with no value.
     */
  public AddressBookFeildValue()
  {
  }

  /**
   * Creates a new <code>AddressBookFeildValue</code> with a given value.
   */
  public AddressBookFeildValue(String value)
  {
    this.value = value;
  }

  /**
   *
   * @return The value that this <code>AddressBookFeildValue</code> represents.
   */
  public String getValue()
  {
    return value;
  }

  /**
   * Is the value hidden. Should the user be allowed to see this value.
   */
  public boolean isHidden()
  {
    return hidden;
  }

  /**
   * Is the value read only. Should the user be permitted to change this value.
   */
  public boolean isReadonly()
  {
    return readonly;
  }

  /**
   * Is the value a system value. That is a special one that the user probably doesn't nead to see.
   */
  public boolean isSystem()
  {
    return system;
  }

  /**
   * Set whether the user be allowed to see this value.
   */
  public void setHidden(boolean hidden)
  {
    this.hidden = hidden;
  }

  /**
   * Set whether the user be permitted to change this value.
   */
  public void setReadonly(boolean readonly)
  {
    this.readonly = readonly;
  }

  /**
   * Set whether it is a special one that the user probably doesn't nead to see.
   */
  public void setSystem(boolean system)
  {
    this.system = system;
  }

  /**
   * Change the value this <code>AddressBookFeildValue</code> represents.
   */
  public void setValue(String value)
  {
    this.value = value;
  }

  /**
   * A string representing this value.
   */
  public String toString()
  {
    return getValue();
  }
}
