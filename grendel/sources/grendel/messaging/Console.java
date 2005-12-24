/*
 * Console.java
 *
 * Created on 19 August 2005, 15:19
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.messaging;

import java.io.PrintStream;

/**
 *
 * @author hash9
 */
public class Console implements Publisher {
    PrintStream ps;
    /** Creates a new instance of Console */
    public Console(PrintStream ps) {
        this.ps = ps;
        ps.println("Publisher: " +toString() + " on " + ps.toString());
    }

    public void publish(Notice notice) {
        if (notice instanceof ExceptionNotice) {
            ((ExceptionNotice)notice).t.printStackTrace();
        } else {
        ps.println(notice.toString());
        }
    }
    
}
