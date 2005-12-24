/*
 * Or.java
 *
 * Created on 01 October 2005, 11:43
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.query;

/**
 *
 * @author hash9
 */
class Or implements Entity
{
  
  private QueryStatement one;
  private QueryStatement two;
  
  /** Creates a new instance of Or */
  public Or(QueryStatement one, QueryStatement two)
  {
    this.one = one;
    this.two = two;
  }
  
  public String makeSQL()
  {
    StringBuilder sb = new StringBuilder();
    sb.append(one.makeSQL());
    sb.append(" OR ");
    sb.append(two.makeSQL());
    return sb.toString();
  }
  
  public String makeLDAP()
  {
    StringBuilder sb = new StringBuilder();
    sb.append("(|(");
    sb.append(one.makeLDAP());
    sb.append(")(");
    sb.append(two.makeLDAP());
    sb.append("))");
    return sb.toString();
  }
  
}
