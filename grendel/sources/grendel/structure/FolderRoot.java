/*
 * FolderRoot.java
 *
 * Created on 20 August 2005, 00:31
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */

package grendel.structure;

/**
 *
 * @author hash9
 */
public class FolderRoot extends Folder {
    /** Creates a new instance of FolderRoot */
    public FolderRoot(Server server, javax.mail.Folder f) {
        super(null,f);
        this.server = server;
    }
    
    public Server getServer() {
        return server;
    }
}
