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
 * $Id: ConsoleWindow.java,v 1.1 2001/07/12 20:25:49 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.awt.*;
import java.awt.event.*;
import java.util.Enumeration;

class ConsoleWindow extends Frame {
    PluggableJVM m_jvm      = null;
    int          debugLevel = 10; 
    /* 
     * Create the new console window
     */
    public ConsoleWindow(PluggableJVM jvm) {
	super("Waterfall Java Console");
	m_jvm = jvm;
	setResizable(true);
	setLayout(new BorderLayout());
	textArea = new TextArea();
	add(textArea, BorderLayout.CENTER);
		
	Button clear = new Button("Clear");
	Button close = new Button("Close");

	Panel panel = new Panel();
	panel.setLayout(new FlowLayout(FlowLayout.CENTER));
	panel.add(clear);
	panel.add(new Label("    "));
	panel.add(close);

	KeyListener keyListener = new KeyAdapter() {
		public void keyPressed(KeyEvent evt) {
		    switch (evt.getKeyCode()) {
		    // those events passed to default handlers
		    case KeyEvent.VK_KP_DOWN:
		    case KeyEvent.VK_KP_UP:
		    case KeyEvent.VK_KP_LEFT:
		    case KeyEvent.VK_KP_RIGHT:
		    case KeyEvent.VK_DOWN:
		    case KeyEvent.VK_UP:
		    case KeyEvent.VK_LEFT:
		    case KeyEvent.VK_RIGHT:
		    case KeyEvent.VK_PAGE_UP:
		    case KeyEvent.VK_PAGE_DOWN:
		    case KeyEvent.VK_END:
		    case KeyEvent.VK_HOME:
			return;
		    }
		    switch (evt.getKeyChar()) {
		    case '?':
		    case 'h':
		    case 'H':
			printHelp();
			break;
		    case 't':
		    case 'T':
			dumpThreads();
			break;
		    case 'g':
		    case 'G':
			System.out.println("running garbage collector...");
			System.gc();
			System.out.println("done.");
			break;
		    case 'f':
		    case 'F':
			System.out.println("finalize objects...");
			System.runFinalization();
			System.out.println("done.");
			break;
		    case 'm':
		    case 'M':
			Runtime r = Runtime.getRuntime();
			System.out.println("Memory usage: free "+
					   r.freeMemory() +
					   " of total " +
					   r.totalMemory());
			break;
		    case 'c':
		    case 'C':
			textArea.setText("");
			break;
		    case 'q':
		    case 'Q':
			hide();
			break;
		    case 'o':
		    case 'O':
			dumpObjs();
			break;
		    case 'V':
			m_jvm.setDebugLevel(++debugLevel);
			System.out.println("new debug level="+debugLevel);
			break;
		    case 'v':
			m_jvm.setDebugLevel(--debugLevel);
			System.out.println("new debug level="+debugLevel);
			break;
		    default:
		    }
		    // don't propagate  further
		    evt.consume();
		}
		public void keyReleased(KeyEvent evt) {
		}
		public void keyTyped(KeyEvent evt) {
		}   
	    };
	addKeyListener(keyListener);
	textArea.addKeyListener(keyListener);

	WindowListener windowEventListener = new WindowAdapter() {
             public void windowClosing(WindowEvent evt) {
                dispose();
            }
        }; 
        addWindowListener(windowEventListener);	


	int rgb = SystemColor.control.getRGB();
	panel.setBackground(new Color(rgb));

	add(panel, BorderLayout.SOUTH);
	
	clear.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e)  {
		    synchronized (textArea) {
			textArea.setText("");
		    };
		}
	    });
	
	close.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e)  {
		    hide();
		}
	    });
	
	setSize(400,300);
	pack();
    }

    void printHelp()
    {
	System.out.println("Waterfall Java Console commands:");
	System.out.println("      h - print this message");
	System.out.println("      t - print all Java threads");
	System.out.println("      g - run garbage collector");
	System.out.println("      f - finalize objects in finalizer queue");
	System.out.println("      m - print memory usage");
	System.out.println("      c - clear console");
	System.out.println("      q - close console");
	System.out.println("      o - show all objects");
	System.out.println("      V/v - increase/decrease verbosity");
    }

    void dumpThreads() 
    {
	System.out.println("Threads list:");
	Thread t = Thread.currentThread();
	ThreadGroup tg = t.getThreadGroup();
	// find root thread group
	while (tg.getParent() != null) tg =  tg.getParent();
	// now dump leafs
	dumpThreadGroup(tg);
    }

    void dumpThreadGroup(ThreadGroup tg)
    {
	if (tg == null) return;
	int cnt = tg.activeCount();
	System.out.println(" Thread group "+tg.getName());	
	Thread[] tlist = new Thread [cnt];
	cnt = tg.enumerate(tlist);
	for (int i=0; i < cnt; i++)
	    System.out.println("    "+tlist[i]);
	int tgcnt = tg.activeGroupCount();
	ThreadGroup[] tglist = new ThreadGroup[tgcnt];
	tgcnt = tg.enumerate(tglist);
	for (int i=0; i < tgcnt; i++)
	    dumpThreadGroup(tglist[i]);
    }

    void dumpObjs()
    {
	System.out.println("Registered extensions:");
	for (Enumeration e = m_jvm.getAllExtensions(); e.hasMoreElements();)
	    {
		WaterfallExtension ext = (WaterfallExtension)e.nextElement();
		if (ext != null) System.out.println("     " + ext);
	    }
	System.out.println("Registered windows:");
	for (Enumeration e = m_jvm.getAllFrames(); e.hasMoreElements();)
	    {
		NativeFrame nf = (NativeFrame)e.nextElement();
		if (nf != null) System.out.println("     " + nf);
	    }
	System.out.println("Registered objects:");
	for (Enumeration e = m_jvm.getAllPeers(); e.hasMoreElements();)
	    {
		HostObjectPeer p = (HostObjectPeer)e.nextElement();
		if (p != null)  System.out.println("     " + p);
	    }
	System.out.println("Registered threads:");
	for (Enumeration e = m_jvm.getAllThreads(); e.hasMoreElements();)
	    {
		NativeThread t = (NativeThread)e.nextElement();
		if (t != null) System.out.println("     " + t);
	    }
	System.out.println("Registered synchro objects:");
	for (Enumeration e = m_jvm.getAllSynchroObjects(); e.hasMoreElements();)
	    {
		SynchroObject so = (SynchroObject)e.nextElement();
		if (so != null) System.out.println("     " + so);
	    }	
	return;
    }

    public void append(String text) {
		synchronized (textArea)  {
			textArea.append(text);
		};
    }

    private TextArea textArea;
}





