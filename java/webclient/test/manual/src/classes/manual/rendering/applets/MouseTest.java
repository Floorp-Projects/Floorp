/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Client QA Team, St. Petersburg, Russia
 */



/*
 * MouseTest.java
 * 
 */
import java.awt.Graphics;
import java.awt.Color;
import java.awt.event.MouseMotionListener;
import java.awt.event.MouseListener;
import java.awt.event.MouseEvent;
import java.awt.event.ActionListener;
import java.awt.event.ActionEvent;
import java.applet.Applet;
import java.awt.TextArea;
import java.awt.Button;
import java.awt.Panel;
import java.awt.BorderLayout;

public class MouseTest extends Applet implements MouseMotionListener, MouseListener
{
    
    private int[] coords = new int[4];
    private boolean inside = false;
    private boolean out = false;
    private int lastX = 0;
    private int lastY = 0;
    private Color defaultColor = Color.red;
    private TextArea console = null;
    private Button reset = null;
    private int appletHeight = 0;
    private int appletWidth = 0;
    private static String msgLast = "";
    private static int msgNumber =0;
    public void init() {
        coords[0] = (new Integer(getParameter("X"))).intValue();
        coords[1] = (new Integer(getParameter("Y"))).intValue();
        coords[2] = (new Integer(getParameter("RecWidth"))).intValue();
        coords[3] = (new Integer(getParameter("RecHeight"))).intValue();
        console = new TextArea("",20,20,TextArea.SCROLLBARS_BOTH);
        reset = new Button("Reset");
        reset.addActionListener(new ActionListener (){
            public void actionPerformed(ActionEvent e) {
                reset();
            }
         });
        setLayout(new BorderLayout());
        Panel p = new Panel();
        p.setLayout(new BorderLayout(0,0));
        p.add(console,BorderLayout.NORTH);
        p.add(reset,BorderLayout.SOUTH);
        add(p,BorderLayout.EAST);
        addMouseMotionListener(this);
        addMouseListener(this);
    }
    public void run() {
    }
    public void paint(Graphics gr) {
        //consolePrint("Paint ...");
        gr.setColor(defaultColor);
        gr.fillRect(coords[0],coords[1],coords[2],coords[3]);
    }

    /*
     ********************************* 
     * Internal methods
     *********************************
    */
    private void reset() {
       coords[0] = (new Integer(getParameter("X"))).intValue();
       coords[1] = (new Integer(getParameter("Y"))).intValue();
       coords[2] = (new Integer(getParameter("RecWidth"))).intValue();
       coords[3] = (new Integer(getParameter("RecHeight"))).intValue();
       console.setText("");
       defaultColor = Color.red;
       repaint();
    }
    private boolean curRectCont(int x,int y) {
        return (coords[0]<=x)&&((coords[0]+coords[2])>=x)&&(coords[1]<=y)&&((coords[1]+coords[3])>=y);
    }
    
    private void consolePrint(String msg) {
        if(msgLast.equals(msg)) {
            msgNumber++;
        } else {
            if(msgNumber>0) {
                console.append("Last message repeated " + msgNumber + 1 + " times\n");
                msgNumber=0;
            }
            msgLast = msg;
            console.append(msg);
            console.append("\n");
        }
    }

    /*
     ********************************* 
     * Listeners section
     *********************************
    */

    /*
     ********************************* 
     * MouseMotionListener section
     *********************************
    */

    public void mouseDragged(MouseEvent e) {
        int deltaX = e.getX() - lastX;
        int deltaY = e.getY() - lastY;
        consolePrint("Mouse dragged");
        inside = curRectCont(e.getX(),e.getY());
        if (inside) {
            coords[0]+= deltaX;
            coords[1]+= deltaY;
            repaint();
        }
        lastX = e.getX();
        lastY = e.getY();
    }


    public void mouseMoved(MouseEvent e) {
         int eventX = e.getX();
         int eventY = e.getY();
         inside = curRectCont(e.getX(),e.getY());
    }

    /*
     ********************************* 
     * MouseListener section
     *********************************
    */

    public void mouseClicked(MouseEvent e) {
        if(inside) {
            consolePrint("Mouse clicked on color rectangle");
            if( defaultColor.equals(Color.blue)) {
                defaultColor = Color.red; 
            } else {
                defaultColor = Color.blue; 
            }
            repaint();
        } else {
            consolePrint("Mouse clicked ...");
        }     
    }

    public void mousePressed(MouseEvent e) {
        if(inside) {
            consolePrint("Mouse pressed on color rectangle");
            lastX=e.getX();
            lastY=e.getY();
        } else {
            consolePrint("Mouse pressed ...");
        }
        
    }
    public void mouseReleased(MouseEvent e) {
        consolePrint("Mouse released ...");
    }

    public void mouseEntered(MouseEvent e) {
        consolePrint("Mouse entered in applet area ...");
    }

    public void mouseExited(MouseEvent e) {
        consolePrint("Mouse exited from applet area ...");
    }


}










