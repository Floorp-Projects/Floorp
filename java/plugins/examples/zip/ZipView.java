import javax.swing.JPanel;
import javax.swing.JTree;
import javax.swing.tree.DefaultMutableTreeNode;
import javax.swing.tree.TreeSelectionModel;
import javax.swing.tree.TreePath;
import javax.swing.JScrollPane;
import javax.swing.JButton;
import javax.swing.JTextField;

import org.mozilla.pluglet.*;
import org.mozilla.pluglet.mozilla.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.print.*;
import java.io.*;
import java.util.zip.*;
import java.util.Enumeration;
import java.util.Vector;

public class ZipView implements PlugletFactory {

    public ZipView() {
    }

    public Pluglet createPluglet(String mimeType) {
 	return new ZipDecoder();
    }

    public void initialize(PlugletManager manager) {	
    }

    public void shutdown() {
    }

}

class Node {
    public DefaultMutableTreeNode parent, name;

    public Node(DefaultMutableTreeNode parent, DefaultMutableTreeNode name) {
	this.parent = parent;
	this.name = name;
    }

}
class ZipDecoder implements Pluglet {
    JButton extract;
    byte[] b;
    Frame frm;
    Panel panel;
    FileDialog fsd;
    JTree zipTree=null;
    Dimension defaultSize;
    String zname;

    public ZipDecoder() {

    }

    public void extractFileEntry(String name, String dir) {
	ZipInputStream zis = new ZipInputStream((InputStream)(new ByteArrayInputStream(b)));
	ZipEntry en;
	try {
	    do {
		en = zis.getNextEntry();
	    } while (!en.toString().equals(name));

	    int size=(int)en.getSize();
	    byte[] buff = new byte[size];

	    String path=dir+name;
	    int last = path.lastIndexOf('/');
	    File dd = null, ff = null;
	    if (last != -1) {
		path=path.substring(0, last);
		dd = new File(path);
		dd.mkdirs();
		ff = new File(dir+name);
	    } else {
		dd = new File(dir);
		dd.mkdirs();
		ff = new File(dir+name);
	    }

	    FileOutputStream fos = new FileOutputStream(ff);
	    int r = 0;
	    int t = 0;
	    do {
		r = zis.read(buff, 0, size);
		fos.write(buff, 0, r);
		t += r;
	    } while (t < size);

	    fos.close();
	    zis.close();

	} catch(Exception e) {
	}

    }

    public void extractEntry(String name, String dir) {
	ZipInputStream zis = new ZipInputStream((InputStream)(new ByteArrayInputStream(b)));
	ZipEntry en;
	try {
	    do {
		en = zis.getNextEntry();
	    } while (!en.toString().equals(name));

	    if (!en.isDirectory()) {
		extractFileEntry(name, dir);
		return;
	    }

	    en = zis.getNextEntry();
	    while (en.toString().indexOf(name) == 0) {
		if(!en.isDirectory())
		    extractFileEntry(en.toString(), dir);
		en = zis.getNextEntry();
	    }
	} catch (Exception e) {
	}
	
    }

    public void processData(byte[] bb, String fname) {
	this.zname = fname;
	this.b = bb;
	showTree();

	extract.addActionListener(new ActionListener() {
	    public void actionPerformed(ActionEvent ae) {
                if (zipTree == null) {
		    return;
		}
		TreePath tp = zipTree.getSelectionPath();
		if (tp == null) {
		    return;
		}
		Object[] elements = tp.getPath();

		if (tp.getPathCount() == 1) return;
		String path = elements[1].toString() + '/';
		for (int i = 2; i < tp.getPathCount(); i++) {
		    path += elements[i].toString()+'/';
		}
		DefaultMutableTreeNode node = (DefaultMutableTreeNode)zipTree.getLastSelectedPathComponent();

		if (node.isLeaf()) {
		    path = path.substring(0, path.length() - 1);
		}
		

		String dir;
		fsd = new FileDialog(frm, "Choose directory to extract", 1);
		fsd.setFile("ZipView");
		fsd.show();
		dir = fsd.getDirectory();
		if (dir == null) {
		    return;
		}

		extractEntry(path, dir);

	    }
	});

    }

    private void addElement(String name, Vector v) {
	Node nn, n;
	DefaultMutableTreeNode newNode = new DefaultMutableTreeNode(name);
	n = (Node)v.lastElement();
	nn = new Node(n.name, newNode);
	n.name.add(newNode);
	v.add(nn);
	 
    }

    private void replaceElement(String name, Vector v, int depth) {
	v.setSize(depth);
	addElement(name, v);
    }

    private void addNode(String path, Vector v) {
	String name=null;
	int start = 0, end = 0, depth = 1;
	Enumeration el;
	Node n, nn;
	boolean vacant=false;

	    el = v.elements();
	    n = (Node)el.nextElement();


	do {
	    end = path.indexOf('/', start);
	    if (end > 0) {
		name = path.substring(start, end);
		start = end+1;
	    }
	    else
		name = path.substring(start); 

	    for (int i=1; i<=depth; i++) {
		if (!el.hasMoreElements()) {
		    addElement(name, v);
		    break;
		} else {
		    n = (Node)el.nextElement();
		    if(n.name.toString().equals(name)) {
			break;
		    }
		    else {
			replaceElement(name, v, depth);
			break;
		    }
		}
	    }
	    depth++;
	} while (end != -1 && start < path.length());

    }

    private void createNodes(DefaultMutableTreeNode top) {
	String name = null;
	Vector v = new Vector(20, 20);


	ZipInputStream zis = new ZipInputStream((InputStream)(new ByteArrayInputStream(b)));
	ZipEntry en = null;

	try {
		v.add(new Node(null, top));
	    do {
		en = zis.getNextEntry();
		if (en != null) {
		    name = en.toString();
		    addNode(name, v);
		}
	    } while (zis.available() == 1);


	} catch (Exception e) {
	}
    }

    private void showTree() {
	DefaultMutableTreeNode top = new DefaultMutableTreeNode(zname);
	createNodes(top);
	
	final JTree tree = new JTree(top);
	zipTree = tree;

	tree.getSelectionModel().setSelectionMode(TreeSelectionModel.SINGLE_TREE_SELECTION);

	JScrollPane treePane = new JScrollPane(tree);
	treePane.setPreferredSize(defaultSize);
	panel.add(treePane);
	panel.setSize(treePane.getPreferredSize());
	frm.pack();
	frm.show();

    }

    public void initialize(PlugletPeer peer) {
	PlugletTagInfo2 info = (PlugletTagInfo2)peer.getTagInfo();
	defaultSize = new Dimension(info.getWidth()-65, info.getHeight()-10);

    }

    public void start() {
	panel = new Panel();
	extract = new JButton("Extract");
    }

    public void stop() {
    }

    public void destroy() {
    }

    public PlugletStreamListener newStream() {
	ZipStreamListener listener = new ZipStreamListener();
	listener.setZip(this);
	return listener;
    }

    public void setWindow(Frame frame) {
	if (frame == null) {
	    return;
	}
	frame.setLayout(new BorderLayout());
	frame.setSize(defaultSize);
	frame.add(panel);
	JPanel jpanel = new JPanel();
	panel.add(jpanel);

	jpanel.add(extract);
	frame.pack();
	frm=frame;
    }

    public void print(PrinterJob printerJob) {
    }

}

class ZipStreamListener implements PlugletStreamListener {
    int total = 0;
    ZipDecoder zip;
    byte[] b, bb;
    Vector v = new Vector(20, 20);

    public ZipStreamListener() {
    }

    public void onStartBinding(PlugletStreamInfo streamInfo) {
	bb = new byte[streamInfo.getLength()];
	total = 0;
    }

    public void setZip(ZipDecoder zip) {
	this.zip=zip;
    }

    public void onDataAvailable(PlugletStreamInfo streamInfo, InputStream input,int  length) {
	String fname;
	fname = streamInfo.getURL();
	fname = fname.substring((fname.lastIndexOf('/'))+1);
	try {
	    int size = input.available();
	    int r = 0;
	    b = new byte[size];

	    r=input.read(b);	

	    for (int i = total; i < total + size; i++) {
		bb[i] = b[i-total];
	    }
            total += r;

	    if (total >= streamInfo.getLength()) {
		input.close();
		zip.processData(bb, fname);
	    }

	} catch(Exception e) {
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






