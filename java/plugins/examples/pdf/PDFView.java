import java.net.URL;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JEditorPane;
import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.print.*;
import java.io.*;
import java.util.Vector;

import com.adobe.acrobat.Viewer;

public class PDFView implements PlugletFactory {
    public PDFView() {
    }
    public Pluglet createPluglet(String mimeType) {
	try {
 	    return new AcrobatView();
	} catch(Exception e) {
	}
	return null;
    }
    public void initialize(PlugletManager manager) {	
    }
    public void shutdown() {
    }
}

class AcrobatView extends Viewer implements Pluglet {
    JScrollPane view;
    com.adobe.acrobat.Viewer acr;
    Dimension defaultSize;
    JPanel panel; 
    Frame frm;

    public void displayPDF(byte[] b) {
	InputStream input = (InputStream)(new ByteArrayInputStream(b));
	frm.removeAll();
	try {
	    acr = new com.adobe.acrobat.Viewer(false);
	    acr.setDocumentInputStream(input);

//	    acr.zoomTo(1.0);
	    acr.activate(); //WithoutBars()
	    acr.execMenuItem(com.adobe.acrobat.ViewerCommand.OneColumn_K);
	    acr.execMenuItem(com.adobe.acrobat.ViewerCommand.FitWidth_K);
	} catch(Exception e) {
	    System.out.println("++ Error loading file.");
	}

	acr.setSize(new Dimension(defaultSize.width, defaultSize.height));
	view.add(acr);
	view.setPreferredSize(defaultSize);
	frm.add(view);
	frm.pack();
	frm.show();
    }


    public AcrobatView() throws Exception {
    }

    public void initialize(PlugletPeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	defaultSize = new Dimension(info.getWidth(), info.getHeight());
    }
    public void start() {
	view = new JScrollPane();
    }
    public void stop() {
	frm.dispose();
	if(acr!=null) acr.deactivate();
    }
    public void destroy() {
	if(acr!=null) acr.destroy();
    }
    public PlugletStreamListener newStream() {
	PDFViewStreamListener listener = new PDFViewStreamListener();
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

class PDFViewStreamListener implements PlugletStreamListener {
    AcrobatView viewer;
    int total = 0, length = 0, first = 0;
    byte[] b, bb;
    Vector v = new Vector(20, 20);

    public void setViewer(AcrobatView view) {
	viewer = view;
    }
    public PDFViewStreamListener() {
    }
    public void onStartBinding(PlugletStreamInfo streamInfo) {
	bb = new byte[streamInfo.getLength()];
	total = 0;
	first = 0;
    }

    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
	try {
	    int size = input.available();
	    int r = 0;
	    b = new byte[size];

	    r=input.read(b);	

	    for(int i=total;i<total+size;i++) {
		bb[i]=b[i-total];
	    }
            total+=r;

	    if(total>=streamInfo.getLength()) {
		input.close();
		viewer.displayPDF(bb);
	    }

	} catch(Exception e) {
	    System.out.println(e.toString());
	}

    }

    public void onFileAvailable(PlugletStreamInfo streamInfo, String fileName) {
    }
    public void onStopBinding(PlugletStreamInfo streamInfo,int status) {
    }
    public int  getStreamType() {
	return 1;
    }
}






