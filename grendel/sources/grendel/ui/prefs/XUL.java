/*
 * XUL.java
 *
 * Created on 13 August 2005, 20:53
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.ui.prefs;
import com.trfenv.parsers.Event;
import com.trfenv.parsers.xul.XulParser;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.Constructor;
import javax.swing.JComponent;
import javax.swing.JFrame;
import org.w3c.dom.Element;



/**
 *
 * @author hash9
 */
public class XUL extends JFrame {
    
    /** Creates a new instance of XUL */
    public XUL() throws FileNotFoundException, IOException {
        XulParser curParser = new XulParser(new Event[0], null);
        //org.w3c.dom.Document doc = curParser.makeDocument("C:\\Documents and Settings\\hash9\\My Documents\\C\\mozilla\\mail\\base\\content\\aboutDialog.xul");
        //org.w3c.dom.Document doc = curParser.makeDocument("C:\\aboutDialog.xul");
        org.w3c.dom.Document doc = curParser.makeDocument("C:\\MainScreen.xml");
        Element frame = (Element)doc.getDocumentElement().getElementsByTagName("window").item(0);
         
        JComponent newPanel = (JComponent)curParser.parseTag(null, frame);
        newPanel.setVisible(true);
        
        /*StringWriter sw = new StringWriter();
        FileReader fr = new FileReader("C:\\Documents and Settings\\hash9\\My Documents\\C\\mozilla\\mail\\base\\content\\aboutDialog.xul");
        int i = 0;
        while (i!=-1) {
            i = fr.read();
            sw.write(i);
        }
        String s = sw.toString();
        //*/
        
        Constructor[] c = XulParser.class.getConstructors();
        for (int i = 0; i<c.length;i++) {
            System.out.println(c[i].toString());
        }
        /*class al_c implements ActionListener {
            public void actionPerformed(ActionEvent e) {
                System.out.println(e.getActionCommand());
            }
        }
        ActionListener al = new al_c();
        ActionListener[] al_a = new ActionListener[0];
        //xulFrame frame = new xulFrame("C:\\Documents and Settings\\hash9\\My Documents\\C\\mozilla\\mail\\base\\content\\aboutDialog.xul",al_a);
        xulFrame frame = new xulFrame("C:\\MainScreen.xml",al_a); 
        frame.show();
        
        Constructor[] c = xulFrame.class.getConstructors();
        for (int i = 0; i<c.length;i++) {
            System.out.println(c[i].toString() +"\t"+c[i].toGenericString());
        }*/
    }
    
    public static void main(String... args) throws FileNotFoundException, IOException {
        new XUL();
    }
}


