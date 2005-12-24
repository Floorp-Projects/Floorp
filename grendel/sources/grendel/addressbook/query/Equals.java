/*
 * Equals.java
 *
 * Created on 01 October 2005, 11:52
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.query;

/**
 *
 * @author hash9
 */
class Equals implements Entity
{
  
  private String feild;
  private String value;
  
  /** Creates a new instance of Equals */
  public Equals(String feild, String value)
  {
    this.feild = feild;
    this.value = value;
  }

    public String makeSQL() {
      return feild +" like '"+value.replace("%","\\%")+"'";
    }
    
        public String makeLDAP() {
      return feild +"="+value.replace("*","\\*");
    }
  
  
}
