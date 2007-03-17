/* 
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
 */

package simple;

import java.awt.Button;
import java.awt.Frame;
import java.awt.List;
import java.awt.Panel;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.print.PrinterJob;
import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import org.mozilla.pluglet.Pluglet;
import org.mozilla.pluglet.PlugletFactory;
import org.mozilla.pluglet.PlugletStreamListener;
import org.mozilla.pluglet.mozilla.PlugletManager;
import org.mozilla.pluglet.mozilla.PlugletPeer;
import org.mozilla.pluglet.mozilla.PlugletStreamInfo;
import org.mozilla.pluglet.mozilla.PlugletTagInfo;
import org.mozilla.pluglet.mozilla.PlugletTagInfo2;


public class SimplePluglet implements PlugletFactory {
    public SimplePluglet() {
	org.mozilla.util.DebugPluglet.print("--test.test()\n");
    }
    /**
     * Creates a new pluglet instance, based on a MIME type. This
     * allows different impelementations to be created depending on
     * the specified MIME type.
     */
    public Pluglet createPluglet(String mimeType) {
	org.mozilla.util.DebugPluglet.print("--test.createPlugletInstance\n");
 	return new SimplePlugletInstance();
    }
    /**
     * Initializes the pluglet and will be called before any new instances are
     * created.
     */
    public void initialize(String plugletPath, PlugletManager manager) {
	org.mozilla.util.DebugPluglet.print("--test.initialize(" + 
					    plugletPath + ")\n");
    }
    /**
     * Called when the browser is done with the pluglet factory, or when
     * the pluglet is disabled by the user.
     */
    public void shutdown() {
	org.mozilla.util.DebugPluglet.print("--test.shutdown\n");
    }
}

class SimplePlugletInstance implements Pluglet {
    Panel panel;
    Button button;
    List list;
    PlugletPeer peer;
    public SimplePlugletInstance() {
	org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.SimplePlugletInstance() \n");
	panel = new Panel();
	button = new Button("Press me :)");
	button.addActionListener(new ActionListener() {
		public void actionPerformed(ActionEvent e) {
		    int t = list.getSelectedIndex();
		    t++; 
		    if (t >= list.getItemCount()) {
			t = 0;
		    }
		    list.select(t);
		}
	    }
				 );
	panel.add(button);
	list = new List(4,false);
	list.add("Pluglet");
	list.add("not");
	list.add("bad");
	list.select(0);
	panel.add(list);
    }
    /**
     * Initializes a newly created pluglet instance, passing to it the pluglet
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer the corresponding pluglet instance peer
     */

    public void initialize(PlugletPeer peer) {
	this.peer = peer;
	org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.initialize\n");
	peer.showStatus("Hello world");
	org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.initialize  "+peer.getMIMEType());
	/*
	try {
	    OutputStreamWriter out = new OutputStreamWriter(peer.newStream("text/plain","_new"));
	    String msg = "Hello, world";
	    out.write(msg,0,msg.length());
	    out.flush();
	    out.close();
	} catch (Exception e) {
	    ;
	}
	*/
    }
    /**
     * Called to instruct the pluglet instance to start. This will be called after
     * the pluglet is first created and initialized, and may be called after the
     * pluglet is stopped (via the Stop method) if the pluglet instance is returned
     * to in the browser window's history.
     */
    public void start() {
	org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.start\n");
    }
    /**
     * Called to instruct the pluglet instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the pluglet goes into the window's history
     * list.
     */
    public void stop() {
       org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.stop\n");
    }
    /**
     * Called to instruct the pluglet instance to destroy itself. This is called when
     * it become no longer possible to return to the pluglet instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     */
    public void destroy() {
       org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.destroy\n");
    }
    /**
     * Called to tell the pluglet that the initial src/data stream is
     * ready. 
     * 
     * @result PlugletStreamListener
     */
    public PlugletStreamListener newStream() {
	org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.newStream\n");
	return new TestStreamListener();
    }
    /**
     * Called when the window containing the pluglet instance changes.
     *
     * @param frame the pluglet panel
     */
    public void setWindow(Frame frame) {
	org.mozilla.util.DebugPluglet.print("--Test...SetWindow "+frame+"\n");
	if(frame == null) {
	    return;
        }
        PlugletTagInfo info = peer.getTagInfo();
	if (info instanceof PlugletTagInfo2) {
	    PlugletTagInfo2 info2 = (PlugletTagInfo2)info;
	    frame.setSize(info2.getWidth(),info2.getHeight());
	    org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.SetWindow width "+info2.getWidth()+ " height "+info2.getHeight()+"\n");
	    org.mozilla.util.DebugPluglet.print("--Test...SetWindow "+frame+"\n");
	}
	frame.add(panel);
	frame.pack();
	frame.show();
    }
     /**
     * Called to instruct the pluglet instance to print itself to a printer.
     *
     * @param print printer information.
     */ 
    public void print(PrinterJob printerJob) {
    }
    
    protected void finalize() {
        org.mozilla.util.DebugPluglet.print("--SimplePlugletInstance.finalize()\n");
    }
}

class TestStreamListener implements PlugletStreamListener {
    public TestStreamListener() {
	org.mozilla.util.DebugPluglet.print("--TestStreamListener.TestStreamListener()\n");
    }
    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     */
    public void onStartBinding(PlugletStreamInfo streamInfo) {
	org.mozilla.util.DebugPluglet.print("--TestStreamListener.onStartBinding ");
	org.mozilla.util.DebugPluglet.print("length "+streamInfo.getLength());
	org.mozilla.util.DebugPluglet.print(" contenet type "+ streamInfo.getContentType());
        org.mozilla.util.DebugPluglet.print(" url "+ streamInfo.getURL());
        org.mozilla.util.DebugPluglet.print(" seekable "+ streamInfo.isSeekable());
        
    }
    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param input  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     */
    public void onDataAvailable(PlugletStreamInfo plugletInfo, InputStream input,int  length) {
	try{
	    org.mozilla.util.DebugPluglet.print("--TestStreamListener.onDataAvailable ");
	    org.mozilla.util.DebugPluglet.print("--length "+input.available()+"\n");
            String cur = null;
            BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(input));
            while (null != (cur = bufferedReader.readLine())) {
                org.mozilla.util.DebugPluglet.print(cur);
            }
	} catch(Exception e) {
	    ;
	}
    }
    public void onFileAvailable(PlugletStreamInfo plugletInfo, String fileName) {
	org.mozilla.util.DebugPluglet.print("--TestStreamListener.onFileAvailable\n");
        org.mozilla.util.DebugPluglet.print(" fileName" + fileName);
    }
    /**
     * Notify the observer that the URL has finished loading. 
     */
    public void onStopBinding(PlugletStreamInfo plugletInfo,int status) {
	org.mozilla.util.DebugPluglet.print("--TestStreamListener.onStopBinding\n");
    }
    public int  getStreamType() {
	org.mozilla.util.DebugPluglet.print("--TestStreamListener.getStreamType\n");
	return 1; //:)
    }
}






