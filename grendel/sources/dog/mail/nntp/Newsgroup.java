/*
 * Newsgroup.java
 *
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
 * The Original Code is Knife.
 *
 * The Initial Developer of the Original Code is dog.
 * Portions created by dog are
 * Copyright (C) 1998 dog <dog@dog.net.uk>. All
 * Rights Reserved.
 *
 * Contributor(s):  Edwin Woudt <edwin@woudt.nl>
 */

package dog.mail.nntp;

import java.io.*;
import java.util.*;
import javax.mail.*;
import javax.mail.event.*;

/**
 * The folder class implementing the NNTP mail protocol.
 *
 * @author dog@dog.net.uk
 * @version 1.0
 */
public class Newsgroup extends Folder {

	String name;
	int count = -1;
	int first = -1;
	int last = -1;
	boolean postingAllowed = false;
	boolean subscribed = false;
	String newsrcline;

	boolean open = false;

	Article[] articles;
	Date checkpoint;

	/**
	 * Constructor.
	 */
	protected Newsgroup(Store store, String name) {
		super(store);
		this.name = name;
	}

	/**
	 * Constructor.
	 */
	protected Newsgroup(Store store, String name, int count, int first, int last) {
		super(store);
		this.name = name;
		this.count = count;
		this.first = first;
		this.last = last;
	}

	/**
	 * Returns the name of this folder.
	 */
	public String getName() {
		return name;
	}

	/**
	 * Returns the full name of this folder.
	 */
	public String getFullName() {
		return getName();
	}

	/**
	 * Returns the type of this folder.
	 */
	public int getType() throws MessagingException {
		return HOLDS_MESSAGES;
	}

	/**
	 * Indicates whether this folder exists.
	 */
	public boolean exists() throws MessagingException {
		if (open) return true;
		try {
			open(READ_ONLY);
			close(false);
			return true;
		} catch (MessagingException e) {
			return false;
		}
	}

	/**
	 * Indicates whether this folder contains any new articles.
	 */
	public boolean hasNewMessages() throws MessagingException {
		return getNewMessageCount()>0;
	}

	/**
	 * Opens this folder.
	 */
	public void open(int mode) throws MessagingException {
		if (open)
			throw new MessagingException("Newsgroup is already open");
		if (mode!=READ_ONLY)
			throw new MessagingException("Newsgroup is read-only");
		((NNTPStore)store).open(this);
		open = true;
		notifyConnectionListeners(ConnectionEvent.OPENED);
		
	}

	/**
	 * Closes this folder.
	 */
	public void close(boolean expunge) throws MessagingException {
		if (!open)
			throw new MessagingException("Newsgroup is not open");
        if (expunge) expunge();
		((NNTPStore)store).close(this);
		open = false;
		notifyConnectionListeners(ConnectionEvent.CLOSED);
	}

	/**
	 * Expunges this folder.
	 */
	public Message[] expunge() throws MessagingException {
        throw new MessagingException("Newsgroup is read-only");	
	}

	/**
	 * Indicates whether this folder is open.
	 */
	public boolean isOpen() {
		return open;
	}

	/**
	 * Indicates whether this folder is subscribed.
	 */
	public boolean isSubscribed() {
		return subscribed;
	}

	/**
	 * Returns the permanent flags for this folder.
	 */
	public Flags getPermanentFlags() { return new Flags(); }
	
	/**
	 * Returns the number of articles in this folder.
	 */
	public int getMessageCount() throws MessagingException {
		return getMessages().length;
	}

	/**
	 * Returns the articles in this folder.
	 */
	public Message[] getMessages() throws MessagingException {
		if (!open)
			throw new MessagingException("Newsgroup is not open");
        NNTPStore s = (NNTPStore)store;
		if (articles==null)
			articles = s.getArticles(this);
		//else { // check for new articles
		//	Article[] nm = s.getNewArticles(this, checkpoint);
		//	if (nm.length>0) {
		//		Article[] m2 = new Article[articles.length+nm.length];
		//		System.arraycopy(articles, 0, m2, 0, articles.length);
		//		System.arraycopy(nm, 0, m2, articles.length, nm.length);
		//		articles = m2;
		//	}
		//}
		//checkpoint = new Date();
        return articles;
	}
	
	/**
	 * Returns the specified article in this folder.
	 * Since NNTP articles are not stored in sequential order,
	 * the effect is just to reference articles returned by getMessages().
	 */
	public Message getMessage(int msgnum) throws MessagingException {
		return getMessages()[msgnum-1];
	}
	
	/**
	 * NNTP folders are read-only.
	 */
	public void appendMessages(Message articles[]) throws MessagingException {
		throw new MessagingException("Folder is read-only");
	}

	/**
	 * Does nothing.
	 */
	public void fetch(Message articles[], FetchProfile fetchprofile) throws MessagingException {
	}

	// -- These must be overridden to throw exceptions --

	/**
	 * NNTP folders can't have parents.
	 */
	public Folder getParent() throws MessagingException {
		throw new MessagingException("Newsgroup can't have a parent");
	}

	/**
	 * NNTP folders can't contain subfolders.
	 */
	public Folder[] list(String s) throws MessagingException {
		throw new MessagingException("Newsgroups can't contain subfolders");
	}

	/**
	 * NNTP folders can't contain subfolders.
	 */
	public Folder getFolder(String s) throws MessagingException {
		throw new MessagingException("Newsgroups can't contain subfolders");
	}

	/**
	 * NNTP folders can't contain subfolders.
	 */
	public char getSeparator() throws MessagingException {
		throw new MessagingException("Newsgroups can't contain subfolders");
	}

	/**
	 * NNTP folders can't be created, deleted, or renamed.
	 */
	public boolean create(int i) throws MessagingException {
		throw new MessagingException("Newsgroups can't be created");
	}

	/**
	 * NNTP folders can't be created, deleted, or renamed.
	 */
	public boolean delete(boolean flag) throws MessagingException {
		throw new MessagingException("Newsgroups can't be deleted");
	}

	/**
	 * NNTP folders can't be created, deleted, or renamed.
	 */
	public boolean renameTo(Folder folder) throws MessagingException {
		throw new MessagingException("Newsgroups can't be renamed");
	}

}
