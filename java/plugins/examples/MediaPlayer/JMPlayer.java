import javax.media.*;
import javax.media.util.*;
import javax.media.format.*;
import javax.media.bean.playerbean.*;
import javax.media.control.*;


import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import org.mozilla.pluglet.mozilla.PlugletTagInfo2;
import java.awt.*;
import java.awt.event.*;
import java.awt.print.*;
import java.io.*;
import java.util.*;


public class JMPlayer implements Pluglet {
    public JMPlayer() {
    }

    public PlugletInstance createPlugletInstance(String mimeType) {
 	return new JMPlayerInstance();
    }

    public void initialize(PlugletManager manager) {	
    }

    public void shutdown() {
    }
}

class JMPlayerInstance implements PlugletInstance, ControllerListener {
    PlugletInstancePeer peer;
    Dimension defaultSize;
    Frame frm;
    int w, h;
    MediaPlayer player = null;

    Panel panel;

    public synchronized void controllerUpdate(ControllerEvent ce) {
	if(ce instanceof RealizeCompleteEvent) {
	    player.prefetch();
	}
	if(ce instanceof PrefetchCompleteEvent) {
	    Dimension dim = player.getPreferredSize();
	    frm.pack();
	    panel.setSize(dim);
//	    player.setBounds(0, 0, w, h);
	    frm.setSize(defaultSize);
	    player.start();
	    frm.show();
	}
    }

    public JMPlayerInstance() {
    }

    public void initialize(PlugletInstancePeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	w = info.getWidth();
	h = info.getHeight();
	defaultSize = new Dimension(w, h);
	this.peer = peer;
    }

    public boolean playFile(String url) {
	player.setMediaLocator(new MediaLocator(url));
	if(player.getPlayer() == null) {
	    return false;
	} else {
	    player.addControllerListener(this);
	    player.realize();
	}
	return true;
    }
	
    public void start() {
	player = new MediaPlayer();
	panel = new Panel();
	panel.add(player);
    }

    public void stop() {
	(new Exception()).printStackTrace();
	player.stop();
	player.deallocate();
	player.close();
    }

    public void destroy() {
    }

    public PlugletStreamListener newStream() {
	org.mozilla.util.Debug.print("--TestInstance.newStream\n");
	JMPlayerStreamListener listener = new JMPlayerStreamListener();
	listener.setPlayer(this);
	return listener;
    }

    public void setWindow(Frame frame) {
	if(frame == null) {
	    return;
	}
	if(panel == null) {
	    System.out.println("++ Initialize failed.");
	    return;
	}
//	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
//	defaultSize = new Dimension(info.getWidth(), info.getHeight());
	frame.setSize(defaultSize);
	frame.setLayout(new BorderLayout());
	frame.add(panel);
	frm=frame;
    }

    public void print(PrinterJob printerJob) {
    }
}

class JMPlayerStreamListener implements PlugletStreamListener {
    JMPlayerInstance jmp;
    int total=0;

    public JMPlayerStreamListener() {
    }

    public void onStartBinding(PlugletStreamInfo streamInfo) {


	if(!jmp.playFile(streamInfo.getURL())) {
    	    System.out.println("++ Error starting player ");
    	    return;
	} 

    }

    public void setPlayer(JMPlayerInstance jmp) {
	this.jmp = jmp;
    }

    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
    }

    public void onFileAvailable(PlugletStreamInfo plugletInfo, String fileName) {
    }

    public void onStopBinding(PlugletStreamInfo plugletInfo,int status) {
	    System.out.println("++ On stop binding.");
    }

    public int  getStreamType() {
	return 1;
    }

}






