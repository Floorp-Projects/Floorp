import java.net.URL;
import javax.swing.JPanel;
//import javax.swing.JScrollPane;
import javax.swing.JEditorPane;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.print.*;
import java.io.*;

public class JView implements Pluglet {
    public JView() {
    }
    public PlugletInstance createPlugletInstance(String mimeType) {
 	return new JViewInstance();
    }
    public void initialize(PlugletManager manager) {	
    }
    public void shutdown() {
    }
}

class JViewInstance implements PlugletInstance {
//    JScrollPane view;
    ScrollPane view;
    JEditorPane viewPane;
    Dimension defaultSize;
    JPanel panel; 
    Frame frm;

    public void displayURL(String url) {
	frm.removeAll();
	try {
	    url = url.replace('|', ':');
	    viewPane.setPage(url);
	    viewPane.setEditable(false);
	} catch (IOException e) {
	    System.out.println("Error loading URL "+url);
	}
	view.add(viewPane);
//	viewPane.setPreferredSize(defaultSize);
	view.setSize(defaultSize);
	view.doLayout();
	frm.add(view, BorderLayout.CENTER);
	frm.pack();
	frm.setVisible(true);
	System.out.println("++ Showing");
    }

    public JViewInstance() {
    }

    public void initialize(PlugletInstancePeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	defaultSize = new Dimension(info.getWidth(), info.getHeight());
    }
    public void start() {
	view = new ScrollPane();
	viewPane = new JEditorPane();
    }
    public void stop() {
	frm.dispose();
    }
    public void destroy() {
    }
    public PlugletStreamListener newStream() {
	JViewStreamListener listener = new JViewStreamListener();
	listener.setViewer(this);
	return listener;
    }
    public void setWindow(Frame frame) {
	if (frame == null) {
	    return;
	}
	frm=frame;
	frm.setSize(defaultSize);
    }
    public void print(PrinterJob printerJob) {
    }
}

class JViewStreamListener implements PlugletStreamListener {
    JViewInstance viewer;

    public void setViewer(JViewInstance view) {
	viewer = view;
    }
    public JViewStreamListener() {
    }
    public void onStartBinding(PlugletStreamInfo streamInfo) {
	viewer.displayURL(streamInfo.getURL());
    }
    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
    }
    public void onFileAvailable(PlugletStreamInfo streamInfo, String fileName) {
    }
    public void onStopBinding(PlugletStreamInfo streamInfo,int status) {
    }
    public int  getStreamType() {
	return 1;
    }
}






