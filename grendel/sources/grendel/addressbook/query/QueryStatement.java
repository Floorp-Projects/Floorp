/*
 * Statement.java
 *
 * Created on 01 October 2005, 11:43
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.addressbook.query;

import java.lang.reflect.Array;
import java.util.ArrayList;

/**
 *
 * @author hash9
 */
public class QueryStatement implements Entity
{
  private ArrayList<Entity> el = new ArrayList<Entity>();
  
  public QueryStatement feildEquals(String feild, String value)
  {
    Entity e = new Equals(feild,value);
    el.add(e);
    return this;
  }
  public QueryStatement feildContains(String feild, String value)
  {
    Entity e = new Contains(feild,value);
    el.add(e);
    return this;
  }
  public QueryStatement feildRegex(String feild, String regex)
  {
    Entity e = new Regex(feild,regex);
    el.add(e);
    return this;
  }
  public QueryStatement feildLike(String feild, String like)
  {
    Entity e = new Like(feild,like);
    el.add(e);
    return this;
  }
  public QueryStatement or(QueryStatement one, QueryStatement two)
  {
    Entity e = new Or(one, two);
    el.add(e);
    return this;
  }
  
  public String makeSQL()
  {
    StringBuilder sb = new StringBuilder();
    boolean start = true;
    for (Entity e: el)
    {
      if (!(e instanceof Or))
      {
        if (!start)
        {
          sb.append(" AND ");
        }
      }
      sb.append(e.makeSQL());
      start = false;
    }
    return sb.toString();
  }
  public String makeLDAP()
  {
    if (el.size()==0)
    {
      return "";
    }
    else if (el.size()==1)
    {
      return el.get(0).makeLDAP();
    }
    
    StringBuilder sb = new StringBuilder();
    sb.append("&");
    
    boolean restart = false;
    for (Entity e: el)
    {
      if (e instanceof Or)
      {
        sb.append("(");
        sb.append(e.makeLDAP());
        sb.append(")");
      }
      else
      {
        if (restart)
        {
          sb.append("&");
          restart = false;
        }
        sb.append("(");
        sb.append(e.makeLDAP());
        sb.append(")");
      }
    }
    /*if (! restart)
    {
      sb.append(")");
    }*/
    return sb.toString();
  }
  
  /*feildEquals("name","tom").feildContains("email","mozilla.org");
search.or(search1,search2);
search.feildRegex("address","^france");  */
}
