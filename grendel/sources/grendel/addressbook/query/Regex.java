/*
 * Regex.java
 *
 * Created on 01 October 2005, 11:53
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.query;

/**
 *
 * @author hash9
 */
class Regex implements Entity
{
  
  private String feild;
  private String regex;
  
  /** Creates a new instance of Regex */
  public Regex(String feild, String regex)
  {
    this.feild = feild;
    this.regex = regex;
  }
  
  public String makeSQL()
  {
    return feild +" like '"+regex.replace("%","\\%").replace(".*","%")+"'";
  }
  
  public String makeLDAP()
  {
    return feild +"="+regex;
  }
  
}
