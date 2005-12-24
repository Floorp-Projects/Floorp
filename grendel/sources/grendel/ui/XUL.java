/*
 * XUL.java
 *
 * Created on 29 October 2005, 22:02
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.ui;

import com.trfenv.parsers.Event;
import com.trfenv.parsers.xul.xulFrame;
import javax.swing.JButton;
import javax.swing.UIManager;

/**
 *
 * @author hash9
 */
public class XUL {
    
    /** Creates a new instance of XUL */
    public XUL() {
        xulFrame.setDefaultLookAndFeelDecorated(true);
        xulFrame frame = new xulFrame("C:/test.xul",new Event[0]);
        frame.setVisible(true);
        JButton btnFind = (JButton)frame.getElementById("find");
        System.out.println(btnFind.toString());
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        java.awt.EventQueue.invokeLater(new Runnable() {
            public void run() {
                new XUL();
            }
        });
    }
    
}
