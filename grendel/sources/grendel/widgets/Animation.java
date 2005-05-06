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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Created: Will Scullin <scullin@netscape.com>, 23 Oct 1997.
 */

package grendel.widgets;

import java.awt.Component;
import java.awt.Dimension;
import java.awt.Graphics;
import java.text.MessageFormat;

import javax.swing.Icon;
import javax.swing.ImageIcon;

public class Animation extends Component implements Runnable
{
  Thread fThread;
  Icon fGlyphs[];
  int fCurrent = 0;
	boolean runAnimation = true;

  public void run()
  {
    while (runAnimation)
    {
      try
      {
        Graphics g;
        fThread.sleep(100);
        g = getGraphics();
        if (g != null)
          paint(getGraphics());
        fCurrent++;
        fCurrent %= fGlyphs.length;
        if (fCurrent == 0)
        {
          fCurrent = 1;
        }
      }
      catch (InterruptedException e)
      {
        runAnimation = false;
        fThread = null;
      }
    }
  }

  public void paint(Graphics g)
  {
    super.paint(g);

    Dimension size = getSize();
    int x = (size.width - fGlyphs[0].getIconWidth()) / 2;
    int y = (size.height - fGlyphs[0].getIconHeight()) / 2;
    fGlyphs[fCurrent].paintIcon(this, g, x, y);
  }

  public Dimension getPreferredSize()
  {
    return new Dimension(fGlyphs[0].getIconWidth(),
                         fGlyphs[0].getIconHeight());
  }

  public Dimension getMaximumSize()
  {
    return getPreferredSize();
  }
  public Dimension getMinimumSize()
  {
    return getPreferredSize();
  }

  //
  // IShellAnimation interface impl..
  //

  public Component getComponent()
  {
    return this;
  }

  public void setImageTemplate( String template, int iFrames )
  {
    fGlyphs = new Icon[iFrames];
    for (int i = 0; i < iFrames; i++)
    {
      Integer arg[] = { new Integer(i) };
      String name = MessageFormat.format(template, arg, null);
      fGlyphs[i] = new ImageIcon(name);
    }
  }

  public synchronized void start()
  {
    if (fThread != null) {
      fThread.interrupt( );
			try {
				fThread.join( );
			}
			catch ( InterruptedException ie ) {
				// ignore
			}
    }
    fThread = new Thread(this, "Animation");
    fThread.start();
  }

  public synchronized void stop()
  {
    if (fThread != null) {
			fThread.interrupt( );
			try {
				fThread.join( );
			}
			catch ( InterruptedException ie ) {
				// ignore
			}
      fThread = null;
    }
    fCurrent = 0;
    repaint();
  }
}




