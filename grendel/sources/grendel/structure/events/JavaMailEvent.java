/*
 * JavaMailEvent.java
 *
 * Created on 23 August 2005, 11:42
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure.events;

/**
 *
 * @author hash9
 */
public class JavaMailEvent implements Event {
    private javax.mail.event.MailEvent me;
            
    /** Creates a new instance of JavaMailEvent */
    public JavaMailEvent(javax.mail.event.MailEvent me) {
        this.me = me;
    }
    
    public javax.mail.event.MailEvent getMailEvent() {
        return me;
    }

}
