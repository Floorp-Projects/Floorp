/* -*- Mode: java; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL
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
 * Kieran Maclean <kieran at eternal undonet com>
 *
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */
package grendel.ui;

import com.trfenv.parsers.Event;

import grendel.renderer.Renderer;
import grendel.renderer.StHyperlinkListener;
import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;
import grendel.widgets.StatusEvent;

import java.awt.Font;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import java.util.logging.Level;
import java.util.logging.Logger;

import javax.mail.Message;
import javax.mail.MessagingException;

import javax.swing.BorderFactory;
import javax.swing.JScrollPane;
import javax.swing.JTextPane;
import javax.swing.JToolBar;
import javax.swing.event.ChangeEvent;
import javax.swing.event.EventListenerList;

/**
 *This is the box where a Grendel email message will be displayed.
 */
public class MessagePanel extends GeneralPanel
{
  private static Logger mLogger=Logger.getLogger("grendel.ui.MessagePanel");

  static {
    mLogger.setLevel(Level.OFF);
  }

  EventListenerList fListeners=new EventListenerList();

  //  JTextArea     fTextArea;
  JTextPane fTextArea;
  Message fMessage;
  MessagePanel fPanel;

  //  URLComponent  fViewer;
  Thread fMessageLoadThread;
  Event[] fActions={
      ActionFactory.GetNewMailAction(), ActionFactory.GetComposeMessageAction()
    };
  boolean makeRealHTML;

  /**
   * Constructs a new message panel.
   */
  public MessagePanel()
  {
    fPanel=this;

    makeRealHTML=true;
    fTextArea=new JTextPane();
    fTextArea.putClientProperty(
      JTextPane.HONOR_DISPLAY_PROPERTIES, Boolean.TRUE);
    fTextArea.putClientProperty(JTextPane.W3C_LENGTH_UNITS, Boolean.TRUE);
    fTextArea.setEditable(false);
    fTextArea.setContentType("text/html");
    fTextArea.setFont(new Font("Helvetica", Font.PLAIN, 12));
    fTextArea.setBorder(BorderFactory.createLoweredBevelBorder());

    try {
      fTextArea.setPage("/start.html");
    } catch (IOException ioe) {
      ioe.printStackTrace();
    }

    fTextArea.addHyperlinkListener(new StHyperlinkListener(fTextArea));
    add(new JScrollPane(fTextArea));
  }

  /**
   * Sets message displayed to the given message. If the argument is
   * null, clears message area
   */
  public synchronized void setMessage(Message aMessage)
  {
    if (fMessageLoadThread!=null) {
      mLogger.info("Killing msg thread");
      fMessageLoadThread.interrupt();

      try {
        fMessageLoadThread.join();
      } catch (InterruptedException ie) {
        // ignore
      }

      notifyStatus(fLabels.getString("messageLoadedStatus"));
    }

    fMessageLoadThread=new Thread(new LoadMessageThread(aMessage), "msgLoad");
    mLogger.info("Starting msg thread");
    fMessageLoadThread.start();
  }

  /**
   * Returns the current message
   */
  public Message getMessage()
  {
    return fMessage;
  }

  public boolean isOpaque()
  {
    return true;
  }

  /**
   * Returns the toolbar associated with this panel.
   */
  public JToolBar getToolBar()
  {
    return buildToolBar("messageToolBar", fActions);
  }

  public void addMessagePanelListener(MessagePanelListener l)
  {
    fListeners.add(MessagePanelListener.class, l);
  }

  /**
   * Disposes of message panel resources. Currently kills off
   * message loading thread to avoid having magellan try and
   * access an invalid peer.
   */
  public void dispose()
  {
    synchronized (fPanel) {
      if (fMessageLoadThread!=null) {
        mLogger.info("Killing msg thread");
        fMessageLoadThread.interrupt();

        try {
          fMessageLoadThread.join();
        } catch (InterruptedException ie) {
          // ignore
        }

        fMessageLoadThread=null;
      }
    }
  }

  public void removeMessagePanelListener(MessagePanelListener l)
  {
    fListeners.remove(MessagePanelListener.class, l);
  }

  protected void notifyLoaded()
  {
    Object[] listeners=fListeners.getListenerList();
    ChangeEvent changeEvent=null;

    for (int i=0; i<(listeners.length-1); i+=2) {
      if (listeners[i]==MessagePanelListener.class) {
        // Lazily create the event:
        if (changeEvent==null) {
          changeEvent=new ChangeEvent(this);
        }

        ((MessagePanelListener) listeners[i+1]).messageLoaded(changeEvent);
      }
    }
  }

  protected void notifyLoading()
  {
    Object[] listeners=fListeners.getListenerList();
    ChangeEvent changeEvent=null;

    for (int i=0; i<(listeners.length-1); i+=2) {
      if (listeners[i]==MessagePanelListener.class) {
        // Lazily create the event:
        if (changeEvent==null) {
          changeEvent=new ChangeEvent(this);
        }

        ((MessagePanelListener) listeners[i+1]).loadingMessage(changeEvent);
      }
    }
  }

  protected void notifyStatus(String aStatus)
  {
    Object[] listeners=fListeners.getListenerList();
    StatusEvent event=null;

    for (int i=0; i<(listeners.length-1); i+=2) {
      if (listeners[i]==MessagePanelListener.class) {
        // Lazily create the event:
        if (event==null) {
          event=new StatusEvent(this, aStatus);
        }

        ((MessagePanelListener) listeners[i+1]).messageStatus(event);
      }
    }
  }

  class LoadMessageThread implements Runnable
  {
    Message fMessage;

    LoadMessageThread(Message aMessage)
    {
      fMessage=aMessage;
    }

    public void run()
    {
      synchronized (fPanel) {
        if (fPanel.fMessage==fMessage) {
          fMessageLoadThread=null;

          return; // save ourselves some work
        }

        fPanel.fMessage=fMessage;
      }

      MessageExtra mextra=(fMessage!=null) ? MessageExtraFactory.Get(fMessage)
                                           : null;
      notifyLoading();
      notifyStatus(fLabels.getString("messageLoadingStatus"));

      try {
        synchronized (fTextArea) {
          if (fMessage!=null) {
            fTextArea.setText("");

            try {
              InputStream stream = Renderer.render(fMessage);

              // Okay, let's try to read from the stream and set the
              // text from the InputStream.
              fTextArea.setText("");
              fTextArea.getDocument().putProperty(
                "IgnoreCharsetDirective", Boolean.TRUE);
              fTextArea.read(stream, null);
            } catch (IOException e) {
              e.printStackTrace();
            }
          } else {
            fTextArea.setText("");
            fTextArea.setPage(getClass().getResource("/start.html"));
          }
        }
      } catch (Exception e) {
        notifyStatus(e.toString());
        e.printStackTrace();
        fMessage=null;
      } catch (Error e) {
        notifyStatus(e.toString());
        e.printStackTrace();
        fMessage=null;
      }

      if (fMessage!=null) {
        try {
          System.out.println(fTextArea.getText());
          mextra.setIsRead(true); // ### Probably shouldn't do it
                                  // until we know we got some data...
        } catch (MessagingException e) {
          // ### Do anything here?  For now, we drop it...
        }

        notifyStatus(fLabels.getString("messageLoadedStatus"));
      }

      synchronized (fPanel) {
        fMessageLoadThread=null;
      }

      notifyLoaded();
    }
  }
}


/** Debugging hack. */
class TeeInputStream extends InputStream
{
  InputStream real;
  OutputStream out;

  TeeInputStream(InputStream r, OutputStream o)
  {
    real=r;
    out=o;
  }

  public int read() throws IOException
  {
    int c=real.read();
    out.write((byte) c);

    return c;
  }
}
