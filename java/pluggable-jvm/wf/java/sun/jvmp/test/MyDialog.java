/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: MyDialog.java,v 1.2 2001/07/12 19:58:04 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp.test;

import java.awt.*;
import java.awt.event.*;

class MyDialog extends Frame
{
  private Button b;
  public MyDialog(String text)
  {
    b = new Button(text);
    b.setBounds(0, 0, 40, 40);
    setLayout(new FlowLayout(FlowLayout.CENTER));
    b.addActionListener(new ActionListener()
	{	    
	    public void actionPerformed(ActionEvent e)
	    {
		dispose();
	    }
	    
	});
    setSize(200,150);
    add(b);
    setLocation(200, 200);
    pack();
    show();
  }
}
