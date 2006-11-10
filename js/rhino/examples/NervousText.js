/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

// The Java "NervousText" example ported to JavaScript.
// Compile using java org.mozilla.javascript.tools.jsc.Main -extends java.applet.Applet -implements java.lang.Runnable NervousText.js
/*
Adapted from Java code by
    Daniel Wyszynski
    Center for Applied Large-Scale Computing (CALC)
    04-12-95

    Test of text animation.

    kwalrath: Changed string; added thread suspension. 5-9-95
*/
var Font = java.awt.Font;
var Thread = java.lang.Thread;
var separated;
var s = null;
var killme = null;
var i;
var x_coord = 0, y_coord = 0;
var num;
var speed=35;
var counter =0;
var threadSuspended = false; //added by kwalrath

function init() {
        this.resize(150,50);
        this.setFont(new Font("TimesRoman",Font.BOLD,36));
        s = this.getParameter("text");
        if (s == null) {
            s = "Rhino";
        }
        separated = s.split('');
}

function start() {
        if(killme == null)
        {
        killme = new java.lang.Thread(java.lang.Runnable(this));
        killme.start();
        }
}

function stop() {
        killme = null;
}

function run() {
        while (killme != null) {
        try {Thread.sleep(100);} catch (e){}
            this.repaint();
        }
        killme = null;
}

function paint(g) {
        for(i=0;i<separated.length;i++)
        {
        x_coord = Math.random()*10+15*i;
        y_coord = Math.random()*10+36;
        g.drawChars(separated, i,1,x_coord,y_coord);
        }
}

/* Added by kwalrath. */
function mouseDown(evt, x, y) {
        if (threadSuspended) {
            killme.resume();
        }
        else {
            killme.suspend();
        }
        threadSuspended = !threadSuspended;
    return true;
}

