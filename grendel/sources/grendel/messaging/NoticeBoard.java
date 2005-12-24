/*
 * NoticeBoard.java
 *
 * Created on 19 August 2005, 15:20
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.messaging;

import grendel.ui.error2.NoticeDialog;
import java.util.List;
import java.util.Vector;

/**
 *
 * @author hash9
 */
public final class NoticeBoard {
    private static NoticeBoard board = null;
    private List<Publisher> publishers = new Vector<Publisher>();
    
    /** Creates a new instance of NoticeBoard */
    private NoticeBoard() {
        addPublisher(new Console(System.err));
        { // XXX Enable Fancy Dialog where GroupLayout is availible otherwise use the JOptionPane.
            try {
                Class.forName("org.jdesktop.layout.GroupLayout");
                addPublisher(new NoticeDialog());
            } catch (ClassNotFoundException ex) {
                addPublisher(new UI());
            }
        }
        
    }
    
    public static NoticeBoard getNoticeBoard() {
        if (board == null) {
            board = new NoticeBoard();
        }
        return board;
    }
    
    public void addPublisher(Publisher pub) {
        publishers.add(pub);
    }
    
    public void removePublisher(Publisher pub) {
        publishers.remove(pub);
    }
    
     public List<Publisher> listPublishers() {
        return new Vector<Publisher>(publishers);
    }
     
     public void clearPublishers() {
        publishers.clear();
    }
     
    
    public static void publish(Notice notice) {
        if (board == null) {
            board = new NoticeBoard();
        }
        for (Publisher pub: board.publishers) {
            Thread t =new Sender(pub, notice);
            t.start();
        }
    }
}


class Sender extends Thread {
    Publisher pub;
    Notice notice;
    
    Sender(Publisher pub, Notice notice) {
        this.pub = pub;
        this.notice = notice;
    }
    
    public void run() {
        pub.publish(notice);
    }
}