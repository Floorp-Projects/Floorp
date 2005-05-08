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
 * Created: Will Scullin <scullin@netscape.com>, 18 Nov 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Giao Nguyen <grail@cafebabe.org>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.util.Enumeration;
import java.util.Vector;

import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.ui.XMLMenuBuilder;

import com.trfenv.parsers.Event;

public class MessageFrame extends GeneralFrame {
  static Vector fMessageFrames = new Vector();
  MessagePanel  fMessagePanel;

  /**
   * Creates a MessageFrame displaying the given message.
   */

  public MessageFrame(Message aMessage) {
    super("messageFrameLabel", "message");

    fMessagePanel = new MessagePanel();

    fPanel.add(fMessagePanel);
    //    fMenu = buildMenu("messageMain", Util.MergeActions(actions,
    //                                           fMessagePanel.getActions()));
    XMLMenuBuilder builder = new XMLMenuBuilder(Util.MergeActions(actions,
                                        fMessagePanel.getActions()));
    fMenu = builder.buildFrom("ui/menus.xml", this);
    getRootPane().setJMenuBar(fMenu);

    fToolBar = fMessagePanel.getToolBar();
    //    fToolBar.addItem(ToolbarFactory.MakeINSToolbarItem(ToolBarLayout.CreateSpring(),
    //                                                       null));
//    fToolBar.addItem(ToolbarFactory.MakeINSToolbarItem(fAnimation, null));
    fToolBarPanel.setComponent(fToolBar);

    fStatusBar = buildStatusBar();
    fPanel.add(BorderLayout.SOUTH, fStatusBar);

    setMessage(aMessage);

    restoreBounds();

    fMessageFrames.addElement(this);
  }

  public void dispose() {
    saveBounds();
    fMessageFrames.removeElement(this);

    fMessagePanel.dispose();

    super.dispose();
  }

  /**
   * Returns the current message, which may not be fully loaded or
   * displayed.
   */

  public Message getMessage() {
    return fMessagePanel.getMessage();
  }

  /**
   * Sets the current message, which will result in the message being
   * loaded and displayed.
   */

  public void setMessage(Message aMessage) {
    setTitle(fLabels.getString("messageFrameLabel"));
    fMessagePanel.setMessage(aMessage);
    if (aMessage != null) {
      try {
        setTitle(aMessage.getSubject() +
                 fLabels.getString("messageSuffixFrameLabel"));
      } catch (MessagingException e) {
      }
    }
  }

  /**
   * Returns the frame displaying, or in the process of displaying,
   * the given message.
   */

  public static MessageFrame FindMessageFrame(Message aMessage) {
    Enumeration frames = fMessageFrames.elements();
    while (frames.hasMoreElements()) {
      MessageFrame frame = (MessageFrame) frames.nextElement();
      if (frame.getMessage() == aMessage) {
        return frame;
      }
    }
    return null;
  }

  Event actions[] = { ActionFactory.GetExitAction(),
                       ActionFactory.GetComposeMessageAction() };
}
