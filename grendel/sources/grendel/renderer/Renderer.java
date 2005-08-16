/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Render.java
 *
 * Created on 07 August 2005, 17:09
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is
 * Kieran Maclean.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package grendel.renderer;

import com.sun.mail.imap.IMAPNestedMessage;

import grendel.renderer.tools.RenderMessage;

import grendel.storage.MessageExtra;
import grendel.storage.MessageExtraFactory;

import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.io.PrintStream;

import java.util.HashMap;
import java.util.Properties;
import java.util.Set;

import javax.mail.BodyPart;
import javax.mail.Message;
import javax.mail.MessagingException;
import javax.mail.Part;
import javax.mail.Session;
import javax.mail.internet.MimeMessage;
import javax.mail.internet.MimeMultipart;


/**
 *
 * @author hash9
 */
public class Renderer implements Runnable
{
  static {
    renderers=new HashMap<Class,ObjectRender>();
    buildParseTree();
  }

  private static HashMap<Class,ObjectRender> renderers;
  protected Message message;
  protected MessageExtra message_extra;
  protected PipedInputStream inpipe;
  protected PipedOutputStream outpipe;

  private Renderer(Message message) throws IOException
  {
    this.message=message;
    this.message_extra=MessageExtraFactory.Get(message);
    inpipe=new PipedInputStream();
    outpipe=new PipedOutputStream(inpipe);

    Thread t=new Thread(this);
    t.start();
  }

  public static InputStream render(Message message) throws IOException
  {
    Renderer r=new Renderer(message);

    return r.inpipe;
  }

  public Attachment makeAttachment(String index, Part p)
                            throws MessagingException
  {
    StringBuilder url=new StringBuilder();
    url.append("attachment://");
    url.append(message.getFolder().getURLName().toString());
    url.append("/");
    url.append(message_extra.getMessageID());

    //url.append("/");
    url.append(index);

    return new Attachment(p, url.toString());
  }

  public StringBuilder makeAttachmentBox(String index, Part p)
                                  throws MessagingException
  {
    StringBuilder buf=new StringBuilder();
    buf.append("<br>\n<hr>\n<br>\n");

    Attachment a=makeAttachment(index, p);
    buf.append("<table border=\"1\" cellspacing=\"0\">");
    buf.append("<tr><td>");
    buf.append(HTMLUtils.genHRef("<img src=\"image.jpg\">", a.getURL()));
    buf.append("</td><td><table border=\"0\">\n");

    if (p.getFileName()!=null) {
      buf.append(HTMLUtils.genRow("Name", p.getFileName(), a.getURL()));
    }

    if (p.getContentType()!=null) {
      buf.append(HTMLUtils.genRow("Type", p.getContentType(), a.getURL()));
    }

    if (p.getDescription()!=null) {
      buf.append(HTMLUtils.genRow(
                                  "Description", p.getDescription(), a.getURL()));
    }

    buf.append("</table></td></tr>\n");
    buf.append("</table>\n");

    return buf;
  }

  public StringBuilder objectRenderer(Object o, String index, Part p)
                               throws MessagingException
  {
    StringBuilder buf=new StringBuilder();

    try {
      if (o instanceof MimeMultipart) {
        MimeMultipart mm_i=(MimeMultipart) o;
        StringBuilder sb=decend(mm_i, index);
        buf.append(sb);
      } else if (o instanceof IMAPNestedMessage) {
        Object o_1=((Message) o).getContent();

        if (o_1 instanceof String) {
          String s=(String) o_1;
          buf.append("<br>\n<hr>\n<br>\n");

          Message m=new MimeMessage(
                                    Session.getInstance(new Properties()),
                                    new ByteArrayInputStream(s.getBytes()));
          buf.append(objectRenderer(m, index, m));
        } else if (o_1 instanceof MimeMultipart) {
          buf.append(new RenderMessage().objectRenderer(
                                                        (Message) o, index,
                                                        (Message) o, this));
        }
      } else {
        ObjectRender or=renderers.get(o.getClass());

        if (or==null) {
          Set<Class> classes=renderers.keySet();

          for (Class c : classes) {
            if (c.isAssignableFrom(o.getClass())) {
              or=renderers.get(c);

              break;
            }
          }
        }

        if (or==null) {
          buf.append(makeAttachmentBox(index, p));
        } else {
          buf.append(or.objectRenderer(o, index, p, this));
        }
      }
    } catch (IOException ioe) {
      throw new MessagingException("I/O error", ioe);
    }

    return buf;
  }

  /**
   * When an object implementing interface <code>Runnable</code> is used
   * to create a thread, starting the thread causes the object's
   * <code>run</code> method to be called in that separately executing
   * thread.
   * <p>
   * The general contract of the method <code>run</code> is that it may
   * take any action whatsoever.
   *
   * @see     java.lang.Thread#run()
   */
  public void run()
  {
    PrintStream ps=new PrintStream(outpipe, true);
    ps.println("<html>");
    ps.println("<head>");
    ps.println("</head>");
    ps.println("<body>");

    try {
      ps.println(objectRenderer(message, null, message).toString());
    } catch (Exception e) {
      e.printStackTrace();
      e.printStackTrace(ps);
    }

    ps.println("</body>");
    ps.println("</html>");
    ps.close();
  }

  private static void buildParseTree()
  {
    try {
      String name="/grendel/renderer/tools";

      // Get a File object for the package
      String s=Renderer.class.getResource(name).getFile();
      s=s.replace("%20", " ");

      File directory=new File(s);
      System.out.println("dir: "+directory.toString());

      if (directory.exists()) {
        // Get the list of the files contained in the package
        String[] files=directory.list();

        for (int i=0; i<files.length; i++) {
          // we are only interested in .class files
          if (files[i].endsWith(".class")) {
            // removes the .class extension
            String classname=files[i].substring(0, files[i].length()-6);

            try {
              // Try to create an instance of the object
              Object o=Class.forName("grendel.renderer.tools."+classname)
                       .newInstance();

              if (o instanceof ObjectRender) {
                ObjectRender or=(ObjectRender) o;
                renderers.put(or.acceptable(), or);
              }
            } catch (ClassNotFoundException cnfex) {
              System.err.println(cnfex);
            } catch (InstantiationException iex) {
              // We try to instantiate an interface
              // or an object that does not have a
              // default constructor
            } catch (IllegalAccessException iaex) {
              // The class is not public
            }
          }
        }
      }
    } catch (Exception e) {
      e.printStackTrace();
    }
  }

  private StringBuilder decend(MimeMultipart mm, String index)
                        throws MessagingException, IOException
  {
    StringBuilder buf=new StringBuilder();
    int max=mm.getCount();

    for (int i=0; i<max; i++) {
      BodyPart bp=mm.getBodyPart(i);
      Object o=bp.getContent();
      buf.append(objectRenderer(o, index+"/"+i, bp));
    }

    return buf;
  }
}
