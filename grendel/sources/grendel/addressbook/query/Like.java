/*
 * Like.java
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
class Like implements Entity
{
  
  private String feild;
  private String like;
  
  /** Creates a new instance of Like */
  public Like(String feild, String like)
  {
    this.feild = feild;
    this.like = like;
  }

    public String makeSQL() {
      return feild +" like '"+like+"'";
    }
    
    public String makeLDAP() {
      return feild +"~="+like.replace("*","\\*").replace("%","*");
    }
    
  
}
