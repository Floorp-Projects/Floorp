/*
 * Publisher.java
 *
 * Created on 19 August 2005, 15:19
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.messaging;

/**
 *
 * @author hash9
 */
public interface Publisher {
    
    public void publish(Notice notice);
    
}
