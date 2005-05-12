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
 * Created: Will Scullin <scullin@netscape.com>,  3 Sep 1997.
 *
 * Contributors: Jeff Galyan <talisman@anamorphic.com>
 *               Edwin Woudt <edwin@woudt.nl>
 */

package grendel.ui;

import java.awt.BorderLayout;
import java.awt.Component;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.io.PrintStream;
import java.io.StringReader;
import java.net.URL;
import java.net.MalformedURLException;
import java.util.Locale;
import java.util.ResourceBundle;

import javax.swing.Action;
import javax.swing.AbstractAction;
import javax.swing.BorderFactory;
import javax.swing.JTextPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
//import javax.swing.JToolBar;
import javax.swing.UIManager;
import javax.swing.event.ChangeEvent;
import javax.swing.event.EventListenerList;
import javax.swing.text.Document;
import javax.swing.text.BadLocationException;
import javax.swing.text.html.*;

//import netscape.orion.toolbars.NSToolbar;

import javax.mail.Message;
import javax.mail.MessagingException;

import grendel.mime.IMimeParser;
import grendel.mime.IMimeOperator;
import grendel.mime.parser.MimeParserFactory;
import grendel.mime.html.MimeHTMLOperatorFactory;
import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;
import grendel.widgets.GrendelToolBar;

import grendel.widgets.StatusEvent;

import com.trfenv.parsers.Event;

/* ####
import calypso.net.URLSource;
*/

import calypso.util.ByteBuf;

import java.util.logging.Logger;
import java.util.logging.Level;

//import netscape.orion.uimanager.IUICmd;

//import lego.document.HTMLDocument;

//import mg.edge.embed.jfc.URLComponent;
//import mg.edge.embed.jfc.URLComponentFactory;
//import mg.magellan.document.IDocument;

/**
 *This is the box where a Grendel email message will be displayed.
 */
public class MessagePanel extends GeneralPanel {
  //  JTextArea     fTextArea;
  JTextPane   fTextArea;
  //  URLComponent  fViewer;
  Thread        fMessageLoadThread;
  Message       fMessage;
  MessagePanel  fPanel;
  boolean makeRealHTML;

  EventListenerList fListeners = new EventListenerList();

  Event              fActions[] = {ActionFactory.GetNewMailAction(),
                                    ActionFactory.GetComposeMessageAction()};

  private static Logger mLogger = Logger.getLogger("grendel.ui.MessagePanel");

  static {
    mLogger.setLevel(Level.OFF);
  }

  /**
   * Constructs a new message panel.
   */

  public MessagePanel() {
    fPanel = this;

    makeRealHTML = true;

    fTextArea = new JTextPane();
    fTextArea.setEditable(false);
    fTextArea.setContentType("text/html");
    fTextArea.setFont(new Font("Helvetica", Font.PLAIN, 12));
    fTextArea.setBorder(BorderFactory.createLoweredBevelBorder());

    add(new JScrollPane(fTextArea));
  }

  /**
   * Disposes of message panel resources. Currently kills off
   * message loading thread to avoid having magellan try and
   * access an invalid peer.
   */

  public void dispose() {
    synchronized (fPanel) {
      if (fMessageLoadThread != null) {
        mLogger.info("Killing msg thread");
        fMessageLoadThread.interrupt();
        try {
          fMessageLoadThread.join();
        }
        catch ( InterruptedException ie ) {
          // ignore
        }
        fMessageLoadThread = null;
      }
    }
  }

  /**
   * Returns the toolbar associated with this panel.
   */

  public GrendelToolBar getToolBar() {
    return buildToolBar("messageToolBar", fActions);
  }

  /**
   * Sets message displayed to the given message. If the argument is
   * null, clears message area
   */

  public synchronized void setMessage(Message aMessage) {
    if (fMessageLoadThread != null) {
      mLogger.info("Killing msg thread");
      fMessageLoadThread.interrupt();
      try {
        fMessageLoadThread.join();
      }
      catch ( InterruptedException ie ) {
        // ignore
      }
      notifyStatus(fLabels.getString("messageLoadedStatus"));
    }
    fMessageLoadThread =
      new Thread(new LoadMessageThread(aMessage), "msgLoad");
    mLogger.info("Starting msg thread");
    fMessageLoadThread.start();
  }

  /**
   * Returns the current message
   */

  public Message getMessage() {
    return fMessage;
  }

  public void addMessagePanelListener(MessagePanelListener l) {
    fListeners.add(MessagePanelListener.class, l);
  }

  public void removeMessagePanelListener(MessagePanelListener l) {
    fListeners.remove(MessagePanelListener.class, l);
  }

  protected void notifyLoading() {
    Object[] listeners = fListeners.getListenerList();
    ChangeEvent changeEvent = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == MessagePanelListener.class) {
        // Lazily create the event:
        if (changeEvent == null)
          changeEvent = new ChangeEvent(this);
        ((MessagePanelListener)listeners[i+1]).loadingMessage(changeEvent);
      }
    }
  }

  protected void notifyLoaded() {
    Object[] listeners = fListeners.getListenerList();
    ChangeEvent changeEvent = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == MessagePanelListener.class) {
        // Lazily create the event:
        if (changeEvent == null)
          changeEvent = new ChangeEvent(this);
        ((MessagePanelListener)listeners[i+1]).messageLoaded(changeEvent);
      }
    }
  }

  protected void notifyStatus(String aStatus) {
    Object[] listeners = fListeners.getListenerList();
    StatusEvent event = null;
    for (int i = 0; i < listeners.length - 1; i += 2) {
      if (listeners[i] == MessagePanelListener.class) {
        // Lazily create the event:
        if (event == null)
          event = new StatusEvent(this, aStatus);
        ((MessagePanelListener)listeners[i+1]).messageStatus(event);
      }
    }
  }

  public boolean isOpaque() {
    return true;
  }

  class LoadMessageThread implements Runnable {
    Message fMessage;

    LoadMessageThread(Message aMessage) {
      fMessage = aMessage;
    }

    public void run() {
      synchronized (fPanel) {
        if (fPanel.fMessage == fMessage) {
          fMessageLoadThread = null;
          return; // save ourselves some work
        }
        fPanel.fMessage = fMessage;
      }
      MessageExtra mextra =
        fMessage != null ? MessageExtraFactory.Get(fMessage) : null;
      notifyLoading();
      notifyStatus(fLabels.getString("messageLoadingStatus"));

      try {
        synchronized (fTextArea) {
          if (fMessage != null) {
            fTextArea.setText("");
            try {
              InputStream stream = mextra.getInputStreamWithHeaders();
              if (makeRealHTML) {
                stream = new MakeItHTML(stream).getHTMLInputStream();
              }
              // Okay, let's try to read from the stream and set the
              // text from the InputStream. We may need to put this
              // stuff back later. (talisman)
              //InputStreamReader reader = new InputStreamReader(stream);
              //  char buff[] = new char[4096];
              // int count;
              // while ((count = reader.read(buff, 0, 4096)) != -1) {
              //   fTextArea.append(new String(buff, 0, count));
              fTextArea.read(stream, "Message");
              //  }
            } catch (MessagingException me) {
              fTextArea.setText(me.toString());
              me.printStackTrace();
            } catch (IOException e) {
              fTextArea.setText(e.toString());
              e.printStackTrace();
            }
          } else {
            fTextArea.setText("This space intentionally left blank");
          }
        }
      } catch (Exception e) {
        notifyStatus(e.toString());
        e.printStackTrace();
        fMessage = null;
      } catch (Error e) {
        notifyStatus(e.toString());
        e.printStackTrace();
        fMessage = null;
      }

      if (fMessage != null) {
        try {
          mextra.setIsRead(true); // ### Probably shouldn't do it
                                  // until we know we got some data...
        } catch (MessagingException e) {
          // ### Do anything here?  For now, we drop it...
        }
        notifyStatus(fLabels.getString("messageLoadedStatus"));
      }

      synchronized (fPanel) {
        fMessageLoadThread = null;
      }
      notifyLoaded();
    }
  }
}

/** Debugging hack. */
class TeeInputStream extends InputStream {
  InputStream real;
  OutputStream out;
  TeeInputStream(InputStream r, OutputStream o) {
    real = r;
    out = o;
  }
  public int read() throws IOException {
    int c = real.read();
    out.write((byte) c);
    return c;
  }
}


class MakeItHTML implements Runnable {
  protected PipedInputStream inpipe;
  protected PipedOutputStream outpipe;
  protected InputStream rawin;

  MakeItHTML(InputStream in) throws IOException {
    rawin = in;
    inpipe = new PipedInputStream();
    outpipe = new PipedOutputStream(inpipe);
    Thread t = new Thread(this);
    t.start();
  }

  InputStream getHTMLInputStream() {
    if (false) {
      return new TeeInputStream(inpipe, System.out);
    } else if (false) {
      try {
        FileOutputStream out = new FileOutputStream("dump.html");
        return new TeeInputStream(inpipe, out);
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
    return inpipe;
  }

  public void run() {
    IMimeParser parser = MimeParserFactory.Make("message/rfc822");
    IMimeOperator op = MimeHTMLOperatorFactory.Make(parser.getObject(),
                                                    new PrintStream(outpipe)
                                                    /*System.out*/);

    parser.setOperator(op);

    byte[] bytes = new byte[1024];
    ByteBuf buf = new ByteBuf(bytes.length);
    int bread;
    do {
      try {
        bread = rawin.read(bytes, 0, bytes.length);
      } catch (IOException e) {
        break;
      }
      if (bread > 0) {
        buf.setLength(0);
        buf.append(bytes, 0, bread);        // this sucks!
        parser.pushBytes(buf);
      }
    } while (bread >= 0);
    bytes = null;
    buf = null;
    parser.pushEOF();
    try {
      outpipe.close();
    } catch (IOException e) {
    }
  }
}




class StupidHackToMakeHTML extends InputStream {
  InputStream fRaw;
  ByteBuf buffer = new ByteBuf();

  StupidHackToMakeHTML(InputStream raw) {
    fRaw = raw;
    buffer.append("<HTML><BODY><PRE>");
  }


  public int read() throws IOException {
    int result;
    if (buffer.length() > 0) {
      result = buffer.byteAt(0);
      buffer.remove(0, 1);
    } else {
      result = fRaw.read();
      if (result == '<') {
        result = '&';
        buffer.append("lt;");
      } else if (result == '>') {
        result = '&';
        buffer.append("gt;");
      } else if (result == '&') {
        buffer.append("amp;");
      } else if (result == '\n') {
        result = '<';
        buffer.append("BR>");
      }
    }
    return result;
  }
}
