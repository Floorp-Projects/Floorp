/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is a test file for Rhino debugger.
 *
 * The Initial Developer of the Original Code is
 * RUnit Software AS.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov, igor@mir2.org
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


// Test file for Rhino debugger 



var x = 12;

function sleep(millis)
{
	java.lang.Thread.sleep(millis);
}

function thread_loop_body(counter, thread_title)
{
	print("["+counter+"] Thread '"+thread_title);
	sleep(1000);
}

function make_thread(thread_title, repeat_count, loop_body)
{
	var Thread = java.lang.Thread;
	var thread = new Thread(function() {
		for (var i = 0; i < repeat_count; ++i) {
			loop_body(i, thread_title);
		}
		print("[DONE] Thread "+thread_title);
	});
	return thread;
}

function make_thread2(thread_title, repeat_count, loop_body, loop_end)
{
	var Thread = java.lang.Thread;
	var thread = new Thread(function() {
		create_gui();
		for (var i = 0; i < repeat_count; ++i) {
			loop_body(i, thread_title);
		}
		loop_end(thread_title);
	});
	return thread;
}


var loop_body_text = thread_loop_body.toSource(0);
var loop_body;
eval("loop_body = "+loop_body_text);

var thread1 = make_thread("A", 5, loop_body);
var thread2 = make_thread("B", 1000, loop_body);
var thread3 = make_thread2("C", 2, loop_body, 
		function loop_end(thread_title) {
			print("[DONE] Thread "+thread_title);
			// Do somethig to throw exception
			Math.xxxx();
	        });

thread1.start();
thread2.start();
thread3.start();

thread1.join();
thread2.join();
thread3.join();


function create_gui()
{
    var swing = Packages.javax.swing;
    var awt = Packages.java.awt;
    
    var frame = new swing.JFrame("SwingApplication");
    var labelPrefix = "Number of button clicks: ";
    var numClicks = 0;
    var label = new swing.JLabel(labelPrefix + numClicks);
    var button = new swing.JButton("Click Me!");
    button.mnemonic = Packages.java.awt.event.KeyEvent.VK_I;
    button.addActionListener(function() {
	numClicks += 1;
	label.setText(labelPrefix + numClicks);
    });
    label.setLabelFor(button);

    var pane = new swing.JPanel();
    pane.setLayout(new awt.GridLayout(0, 1));
    pane.add(button);
    pane.add(label);

    frame.getContentPane().add(pane, awt.BorderLayout.CENTER);
    frame.addWindowListener(function(event, methodName) {
    	print(event + " "+methodName);
	if (methodName == "windowClosing") {     
            java.lang.System.exit(0);
	}
    });

    //Finish setting up the frame, and show it.
    frame.pack();
    frame.setVisible(true);

}

