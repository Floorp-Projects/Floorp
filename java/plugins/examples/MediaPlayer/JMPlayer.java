import javax.media.*;
import javax.media.util.*;
import javax.media.format.*;
import javax.media.bean.playerbean.*;
import javax.media.control.*;


import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.print.*;
import java.io.*;
import java.util.*;


public class JMPlayer implements PlugletFactory {
    public JMPlayer() {
    }
    public Pluglet createPluglet(String mimeType) {
        Pluglet player = null;
        try {
            player = new Player();
        }
        catch (Throwable e) {
            e.printStackTrace();
        }
 	return player;
    }
    public void initialize(String plugletPath, PlugletManager manager) { 
    }
    public void shutdown() {
    }
}

class Player implements Pluglet, ControllerListener {
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
	    frm.setSize(defaultSize);
	    player.start();
	    frm.show();
	}
    }
    public Player() {
    }
    public void initialize(PlugletPeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	w = info.getWidth();
	h = info.getHeight();
	defaultSize = new Dimension(w, h);
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
	player.stop();
	player.close();
    }
    public void destroy() {
    }
    public PlugletStreamListener newStream() {
	JMPlayerStreamListener listener = new JMPlayerStreamListener();
	listener.setPlayer(this);
	return listener;
    }
    public void setWindow(Frame frame) {
	if(frame == null) {
	    return;
	}
	frame.setSize(defaultSize);
	frame.setLayout(new BorderLayout());
	frame.add(panel);
	frm = frame;
    }
    public void print(PrinterJob printerJob) {
    }
}

class JMPlayerStreamListener implements PlugletStreamListener {
    Player jmp;

    public JMPlayerStreamListener() {
    }
    public void onStartBinding(PlugletStreamInfo streamInfo) {
	if(!jmp.playFile(streamInfo.getURL())) {
    	    System.out.println("Error starting player.");
    	    return;
	} 

    }
    public void setPlayer(Player jmp) {
	this.jmp = jmp;
    }
    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
    }
    public void onFileAvailable(PlugletStreamInfo plugletInfo, String fileName) {
    }
    public void onStopBinding(PlugletStreamInfo plugletInfo,int status) {
    }
    public int  getStreamType() {
	return 1;
    }

}






