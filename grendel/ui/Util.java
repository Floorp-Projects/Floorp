/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1997
 * Netscape Communications Corporation.  All Rights Reserved.
 *
 * Created: Will Scullin <scullin@netscape.com>,  9 Sep 1997.
 * Modified: Jeff Galyan <jeffrey.galyan@sun.com>, 31 Dec 1998
 */

package grendel.ui;

import java.awt.Component;
import java.awt.Container;
import java.awt.Frame;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Insets;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.io.UnsupportedEncodingException;
import java.util.Vector;

import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.internet.MimeUtility;

import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JPopupMenu;
import javax.swing.JScrollBar;
import javax.swing.JScrollPane;
//import javax.swing.JToolBar;
import javax.swing.KeyStroke;

import grendel.ui.ToolBarLayout;
import grendel.ui.UIAction;
import grendel.widgets.GrendelToolBar;

public class Util {
  static final boolean DEBUG = true;
  public static final int LEFT = 0;
  public static final int CENTER = 0;
  public static final int RIGHT = 0;

  // Recycled character array
  private static char fChars[] = new char[128];

  public static void DrawTrimmedString(Graphics g, String aString,
                                       FontMetrics aFM, int aWhere,
                                       int x, int y, int aWidth) {
    int w = aFM.stringWidth(aString);
    if (w <= aWidth) {
      g.drawString(aString, x, y);
      return;
    }

    int i;
    int first = 0;
    int length = aString.length();
    if (length > fChars.length) {
      fChars = new char[length];
    }
    aString.getChars(0, length, fChars, 0);

    if (aWhere == RIGHT) {
      fChars[length - 3] = '.';
      fChars[length - 2] = '.';
      fChars[length - 1] = '.';
    } else { // Only left cut for now
      fChars[0] = '.';
      fChars[1] = '.';
      fChars[2] = '.';
    }

    w = aFM.charsWidth(fChars, first, length);

    if (aWhere == RIGHT) {
      while (w > aWidth) {
        length--;
        w -= aFM.charWidth(fChars[length - 3]);
        fChars[length - 3] = '.';
      }
    } else { // Only left cut for now
      while (w > aWidth) {
        length--;
        first++;
        w -= aFM.charWidth(fChars[first + 2]);
        fChars[first + 2] = '.';
      }
    }
    g.drawChars(fChars, first, length, x, y);
  }

  static UIAction FindAction(Vector aVector, String aAction) {
    for (int i = 0; i < aVector.size(); i++) {
      UIAction action = (UIAction)aVector.elementAt(i);
      if (action.equals(aAction)) {
        return action;
      }
    }
    return null;
  }

  static public UIAction[] MergeActions(UIAction aActions1[], UIAction aActions2[]) {
    Vector resVector = new Vector();
    int i;
    if (aActions1 != null) {
      for (i = 0; i < aActions1.length; i++) {
        resVector.addElement(aActions1[i]);
      }
    }
    if (aActions2 != null) {
      for (i = 0; i < aActions2.length; i++) {
        if (FindAction(resVector, aActions2[i].toString()) == null) {
          resVector.addElement(aActions2[i]);
        }
      }
    }
    UIAction res[] = new UIAction[resVector.size()];
    resVector.copyInto(res);
    return res;
  }

  static public GrendelToolBar MergeToolBars(GrendelToolBar aBar1, GrendelToolBar aBar2) {
    GrendelToolBar res = new GrendelToolBar();
    res.setLayout(new ToolBarLayout());
    
    Component barArray1[] = aBar1.getComponents();
    Component barArray2[] = aBar2.getComponents();
    int count1 = aBar1.getComponentCount();
    int count2 = aBar2.getComponentCount();
    int i = 0, j = 0, k, l;
    if (DEBUG) {
      System.out.println("count1 = " + count1 + "; count2 = " + count2);
    }

    while (i < count1) {
      JButton button1 = (JButton) barArray1[i];
      if (j < count2) {
        JButton button2 = (JButton) barArray2[j];
        if (button1.getActionCommand().equals(button2.getActionCommand())) {
          res.add(button1);
          i++;
          j++;
       } else {
            boolean merge = false;
            for (k = j; k < count2; k++) {
              button2 = (JButton) barArray2[k];
              if (button1.getActionCommand().equals(button2.getActionCommand())) {
                merge = true;
                while (j < k) {
                  JButton button3 = (JButton) barArray2[j];
                  res.add(button3);
                  j++;
                }
                break;
              }
            }
          if (merge) {
            res.add(button1);
            j++;
          } else {
            res.add(button1);
          }
          i++;
        }
      } else {
        res.add(button1);
        i++;
      }
    }

    while (j < count2) {
      JButton button2 = null;
      if (barArray2[j] != null)
        {
          button2 = (JButton) barArray2[j];
        }
      if (button2 != null) 
        {
          res.add(button2);
        }
      j++;
    }
    
    return res;
  }
  

  public static void RegisterScrollingKeys(JScrollPane aScrollPane) {
    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_UP),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_UP,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_DOWN),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_DOWN,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_LEFT),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_LEFT,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_RIGHT),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_RIGHT,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_PAGE_UP),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_PAGE_UP,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_PAGE_DOWN),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_PAGE_DOWN,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_HOME),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_HOME,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

    aScrollPane.registerKeyboardAction(new ScrollAction(aScrollPane, KeyEvent.VK_END),
                                       KeyStroke.getKeyStroke(KeyEvent.VK_END,
                                                              KeyEvent.CTRL_MASK),
                                       JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);

  }

  static public Frame GetParentFrame(Component aComponent) {
    Component parent = aComponent.getParent();
    while (parent != null && !(parent instanceof Frame)) {
      if (parent instanceof JPopupMenu) {
        parent = ((JPopupMenu) parent).getInvoker();
      } else {
        parent = parent.getParent();
      }
    }
    return (Frame) parent;
  }

  static String GetSubject(Message aMessage) {
    String result = "";
    try {
      String rawvalue = null;
      try {
        if ((rawvalue = aMessage.getSubject()) != null)
          result = MimeUtility.decodeText(rawvalue);
      } catch (UnsupportedEncodingException e) {
        System.err.println(e + ":" + rawvalue);
        // Don't care
        result = rawvalue;
      }
    } catch (MessagingException e) {
    }
    return result;
  }
}

class ScrollAction extends UIAction {
  JScrollPane fScrollPane;
  int         fAction;

  ScrollAction(JScrollPane aScrollPane, int aAction) {
    super("");
    fScrollPane = aScrollPane;
    fAction = aAction;
  }

  public void actionPerformed(ActionEvent aEvent) {
    JScrollBar vScroll = fScrollPane.getVerticalScrollBar();
    JScrollBar hScroll = fScrollPane.getHorizontalScrollBar();

    switch (fAction) {
    case KeyEvent.VK_UP:
      vScroll.setValue(vScroll.getValue() - vScroll.getUnitIncrement());
      break;
    case KeyEvent.VK_DOWN:
      vScroll.setValue(vScroll.getValue() + vScroll.getUnitIncrement());
      break;
    case KeyEvent.VK_LEFT:
      hScroll.setValue(hScroll.getValue() - hScroll.getUnitIncrement());
      break;
    case KeyEvent.VK_RIGHT:
      hScroll.setValue(hScroll.getValue() + hScroll.getUnitIncrement());
      break;
    case KeyEvent.VK_PAGE_UP:
      vScroll.setValue(vScroll.getValue() - vScroll.getVisibleAmount());
      break;
    case KeyEvent.VK_PAGE_DOWN:
      vScroll.setValue(vScroll.getValue() + vScroll.getVisibleAmount());
      break;
    case KeyEvent.VK_HOME:
      vScroll.setValue(vScroll.getMinimum());
      break;
    case KeyEvent.VK_END:
      vScroll.setValue(vScroll.getMaximum() - vScroll.getVisibleAmount());
      break;
    }
  }
}
