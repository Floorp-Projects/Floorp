/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the GFilt filter package, as integrated into 
 * the Grendel mail/news reader.
 * 
 * The Initial Developer of the Original Code is Ian Clarke.  
 * Portions created by Ian Clarke are
 * Copyright (C) 2000 Ian Clarke.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */


package grendel.filters;  // formerly GFilt

import java.awt.*;
import java.applet.*;
import java.awt.event.*;

public class Applet extends java.applet.Applet
{
	
  /**
   * The constructor, creates the Applet
   */
  public Applet()
  {
  }

  /**
   * Called by the browser or applet viewer to inform 
   * this applet that it has been loaded into the system. It is always 
   * called before the first time that the <code>start</code> method is 
   * called. 
   * <p>
   * A subclass of <code>Applet</code> should override this method if 
   * it has initialization to perform. For example, an applet with 
   * threads would use the <code>init</code> method to create the 
   * threads and the <code>destroy</code> method to kill them. 
   * <p>
   * The implementation of this method provided by the 
   * <code>Applet</code> class does nothing. 
   */
  public void init()
  {
  }

  /**
   * Called by the browser or applet viewer to inform 
   * this applet that it should start its execution. It is called after 
   * the <code>init</code> method and each time the applet is revisited 
   * in a Web page. 
   * <p>
   * A subclass of <code>Applet</code> should override this method if 
   * it has any operation that it wants to perform each time the Web 
   * page containing it is visited. For example, an applet with 
   * animation might want to use the <code>start</code> method to 
   * resume animation, and the <code>stop</code> method to suspend the 
   * animation. 
   * <p>
   * The implementation of this method provided by the 
   * <code>Applet</code> class does nothing. 
   *
   */
  public void start()
  {
  }
	
  /**
   * Called by the browser or applet viewer to inform 
   * this applet that it should stop its execution. It is called when 
   * the Web page that contains this applet has been replaced by 
   * another page, and also just before the applet is to be destroyed. 
   * <p>
   * A subclass of <code>Applet</code> should override this method if 
   * it has any operation that it wants to perform each time the Web 
   * page containing it is no longer visible. For example, an applet 
   * with animation might want to use the <code>start</code> method to 
   * resume animation, and the <code>stop</code> method to suspend the 
   * animation. 
   * <p>
   * The implementation of this method provided by the 
   * <code>Applet</code> class does nothing. 
   *
   */
  public void stop()
  {
  }

}
