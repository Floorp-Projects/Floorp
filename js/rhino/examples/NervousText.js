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

