/*
 * SelectLayoutTask.java
 *
 * Created on 16 October 2005, 22:32
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel;

import grendel.prefs.base.UIPrefs;
import grendel.ui.MessageDisplayManager;
import grendel.ui.MultiMessageDisplayManager;
import grendel.ui.UnifiedMessageDisplayManager;

/**
 *
 * @author hash9
 */
public class SelectLayoutTask extends Task
{
  
  /** Creates a new instance of SelectLayoutTask */
  public SelectLayoutTask()
  {
  }
  
  protected void run() throws Exception
  {
    UIPrefs prefs=UIPrefs.GetMaster();
    MessageDisplayManager fManager;

    if (prefs.getDisplayManager().equals("multi"))
    {
      fManager=new MultiMessageDisplayManager();
    }
    else
    {
      fManager=new UnifiedMessageDisplayManager();
    }
    
    MessageDisplayManager.SetDefaultManager(fManager);
    fManager.displayMaster();
  }
  
}
