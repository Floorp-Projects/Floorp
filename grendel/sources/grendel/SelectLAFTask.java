/*
 * SelectLAFTask.java
 *
 * Created on 16 October 2005, 22:31
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel;

import grendel.prefs.base.UIPrefs;
import javax.swing.LookAndFeel;
import javax.swing.UIManager;

/**
 *
 * @author hash9
 */
public class SelectLAFTask extends Task
{
  
  /** Creates a new instance of SelectLAFTask */
  public SelectLAFTask()
  {
  }

  protected void run() throws Exception
  {
    UIPrefs prefs=UIPrefs.GetMaster();

    UIManager.LookAndFeelInfo[] info=UIManager.getInstalledLookAndFeels();
    LookAndFeel laf;

    for (int i=0; i<info.length; i++) {
      try {
        String name=info[i].getClassName();
        Class c=Class.forName(name);
        laf=(LookAndFeel) c.newInstance();

        if (laf.getDescription().equals(prefs.getLookAndFeel())) {
          UIManager.setLookAndFeel(laf);
        }
      } catch (Exception e) {
      }
    }
  }
  
}
