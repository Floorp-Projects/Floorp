package jmfplayer;

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Frame;
import java.awt.Panel;
import java.awt.print.PrinterJob;
import java.io.InputStream;
import javax.media.ControllerEvent;
import javax.media.ControllerListener;
import javax.media.MediaLocator;
import javax.media.PrefetchCompleteEvent;
import javax.media.RealizeCompleteEvent;
import javax.media.bean.playerbean.MediaPlayer;
import org.mozilla.pluglet.Pluglet;
import org.mozilla.pluglet.PlugletFactory;
import org.mozilla.pluglet.PlugletStreamListener;
import org.mozilla.pluglet.mozilla.PlugletManager;
import org.mozilla.pluglet.mozilla.PlugletPeer;
import org.mozilla.pluglet.mozilla.PlugletStreamInfo;
import org.mozilla.pluglet.mozilla.PlugletTagInfo2;


public class JMFPlayer implements PlugletFactory {
    public JMFPlayer() {
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
	JMFPlayerStreamListener listener = new JMFPlayerStreamListener();
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

class JMFPlayerStreamListener implements PlugletStreamListener {
    Player jmp;

    public JMFPlayerStreamListener() {
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






