/*
 * NNTPStore.java
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
 * Contributor(s): Edwin Woudt <edwin@woudt.nl>
 */

package dog.mail.nntp;

import java.io.*;
import java.net.*;
import java.util.*;
import javax.mail.*;
import javax.mail.event.*;
import javax.mail.internet.*;
import dog.mail.util.*;
import dog.util.*;

/**
 * The storage class implementing the NNTP Usenet news protocol.
 *
 * @author dog@dog.net.uk
 * @version 1.02
 */
public class NNTPStore extends Store implements StatusSource {

	/**
	 * The default NNTP port.
	 */
	public static final int DEFAULT_PORT = 119;

	static int fetchsize = 1024;

	Socket socket;
	CRLFInputStream in;
	CRLFOutputStream out;
	String hostname;

	String response;

    static final int HELP = 100;
    static final int READY = 200;
    static final int READ_ONLY = 201;
    static final int STREAMING_OK = 203;
    static final int CLOSING = 205;
	static final int GROUP_SELECTED = 211; // or list of article numbers follows
	static final int LISTING = 215; // list of newsgroups follows
	static final int ARTICLE_RETRIEVED_BOTH = 220;
	static final int ARTICLE_RETRIEVED_HEAD = 221; // or header follows
	static final int ARTICLE_RETRIEVED_BODY = 222;
	static final int ARTICLE_RETRIEVED = 223;
	static final int LISTING_OVERVIEW = 224;
	static final int LISTING_ARTICLES = 230; // list of new articles by message-id follows
	static final int LISTING_NEW = 231; // list of new newsgroups follows
	static final int ARTICLE_POSTED = 240; // article posted ok
	static final int SEND_ARTICLE = 340; // send article to be posted. End with <CR-LF>.<CR-LF>
    static final int SERVICE_DISCONTINUED = 400;
	static final int NO_SUCH_GROUP = 411;
	static final int NO_GROUP_SELECTED = 412; // no newsgroup has been selected
	static final int NO_ARTICLE_SELECTED = 420; // no current article has been selected
	static final int NO_SUCH_ARTICLE_IN_GROUP = 423; // no such article number in this group
	static final int NO_SUCH_ARTICLE = 430; // no such article found
	static final int POSTING_NOT_ALLOWED = 440; // posting not allowed
	static final int POSTING_FAILED = 441; // posting failed
    static final int COMMAND_NOT_RECOGNIZED = 500;
    static final int COMMAND_SYNTAX_ERROR = 501;
    static final int PERMISSION_DENIED = 502;
    static final int SERVER_ERROR = 503;

	boolean postingAllowed = false;
    Root root;
	Newsgroup current;
    Date lastNewGroup;

	Hashtable newsgroups = new Hashtable(); // hashtable of newsgroups by name
	Hashtable articles = new Hashtable(); // hashtable of articles by message-id

	Vector statusListeners = new Vector();

	/**
	 * Constructor.
	 */
	public NNTPStore(Session session, URLName urlname) {
		super(session, urlname);
		String ccs = session.getProperty("mail.nntp.fetchsize");
		if (ccs!=null) try { fetchsize = Math.max(Integer.parseInt(ccs), 1024); } catch (NumberFormatException e) {}
	}

	/**
	 * Connects to the NNTP server and authenticates with the specified parameters.
	 */
	protected boolean protocolConnect(String host, int port, String username, String password) throws MessagingException {
		if (port<0) port = DEFAULT_PORT;
		if (host==null)
			return false;
		try {
            hostname = host;
			socket = new Socket(host, port);
			in = new CRLFInputStream(new BufferedInputStream(socket.getInputStream()));
			out = new CRLFOutputStream(new BufferedOutputStream(socket.getOutputStream()));

			switch (getResponse()) {
			case READY:
				postingAllowed = true;
			case READ_ONLY:
				//StringTokenizer st = new StringTokenizer(response);
				//if (st.hasMoreTokens()) hostname = st.nextToken();
				break;
			default:
				throw new MessagingException("unexpected server response: "+response);
			}

			send("MODE READER"); // newsreader extension
			switch (getResponse()) {
			case READY:
				postingAllowed = true;
			case READ_ONLY:
				break;
			}

			readNewsrc();

			return true;
		} catch(UnknownHostException e) {
			throw new MessagingException("unknown host", e);
		} catch(IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

	/**
	 * Closes the connection.
	 */
	public synchronized void close() throws MessagingException {
		if (socket!=null)
			try {
				send("QUIT");
				switch (getResponse()) {
				case CLOSING:
					break;
				case SERVER_ERROR:
					if (response.toLowerCase().indexOf("timeout")>-1)
						break;
				default:
					throw new MessagingException("unexpected server response: "+response);
				}
				socket.close();
				socket = null;
			} catch (IOException e) {
				// socket.close() seems to throw an exception!
				//throw new MessagingException("Close failed", e);
			}
		super.close();
	}

	/**
	 * Returns the hostname of the server.
	 */
	public String getHostName() { return hostname; }

	int getResponse() throws IOException {
		response = in.readLine();
		try {
			int index = response.indexOf(' ');
			int code = Integer.parseInt(response.substring(0, index));
			response = response.substring(index+1);
			return code;
		} catch (StringIndexOutOfBoundsException e) {
			throw new ProtocolException("NNTP protocol exception: "+response);
		} catch (NumberFormatException e) {
			throw new ProtocolException("NNTP protocol exception: "+response);
		}
	}

	void send(String command) throws IOException {
		out.write(command.getBytes());
		out.writeln();
		out.flush();
	}

    // Opens a newsgroup.
	synchronized void open(Newsgroup group) throws MessagingException {
		if (current!=null) {
			if (current.equals(group))
				return;
			else
				close(current);
		}
		String name = group.getName();
		try {
			send("GROUP "+name);
            int r = getResponse();
			switch (r) {
			  case GROUP_SELECTED:
				try {
					updateGroup(group, response);
					group.open = true;
					current = group;
				} catch (NumberFormatException e) {
					throw new MessagingException("NNTP protocol exception: "+response, e);
				}
				break;
			  case NO_SUCH_GROUP:
				throw new MessagingException(response);
			  case SERVER_ERROR:
				if (response.toLowerCase().indexOf("timeout")>-1) {
					close();
					connect();
					open(group);
					break;
				}
			  default:
				throw new MessagingException("unexpected server response ("+r+"): "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

    // Updates a newsgroup with the most recent article counts.
	void updateGroup(Newsgroup newsgroup, String response) throws IOException {
		try {
			StringTokenizer st = new StringTokenizer(response, " ");
            newsgroup.count = Integer.parseInt(st.nextToken());
            newsgroup.first = Integer.parseInt(st.nextToken());
			newsgroup.last = Integer.parseInt(st.nextToken());
		} catch (NumberFormatException e) {
			throw new ProtocolException("NNTP protocol exception");
		} catch (NoSuchElementException e) {
			throw new ProtocolException("NNTP protocol exception");
		}
	}

    // Closes a newsgroup.
	synchronized void close(Newsgroup group) throws MessagingException {
		if (current!=null && !current.equals(group)) close(current);
		group.open = false;
	}

    // Returns the (approximate) number of articles in a newsgroup.
	synchronized int getMessageCount(Newsgroup group) throws MessagingException {
        String name = group.getName();
		try {
			send("GROUP "+name);
			switch (getResponse()) {
			  case GROUP_SELECTED:
				try {
					updateGroup(group, response);
					current = group;
					return group.count;
				} catch(NumberFormatException e) {
					throw new MessagingException("NNTP protocol exception: "+response, e);
				}
			  case NO_SUCH_GROUP:
				throw new MessagingException("No such group");
			  case SERVER_ERROR:
				if (response.toLowerCase().indexOf("timeout")>-1) {
					close();
					connect();
					return getMessageCount(group);
				}
			  default:
				throw new MessagingException("unexpected server response: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

    // Returns the headers for an article.
	synchronized InternetHeaders getHeaders(Article article) throws MessagingException {
		String mid = article.messageId;
		if (mid==null) {
			Newsgroup group = (Newsgroup)article.getFolder();
			if (!current.equals(group)) open(group);
			mid = Integer.toString(article.getMessageNumber());
		}
		try {
			send("HEAD "+mid);
			switch (getResponse()) {
			  case ARTICLE_RETRIEVED_HEAD:
				return new InternetHeaders(new MessageInputStream(in));
			  case NO_GROUP_SELECTED:
				throw new MessagingException("No group selected");
			  case NO_ARTICLE_SELECTED:
				throw new MessagingException("No article selected");
			  case NO_SUCH_ARTICLE_IN_GROUP:
				throw new MessagingException("No such article in group");
			  case NO_SUCH_ARTICLE:
				throw new MessagingException("No such article");
			  case SERVER_ERROR:
				if (response.toLowerCase().indexOf("timeout")>-1) {
					close();
					connect();
					return getHeaders(article);
				}
			  default:
				throw new MessagingException("unexpected server response: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

    // Returns the content for an article.
	synchronized byte[] getContent(Article article) throws MessagingException {
		String mid = article.messageId;
		if (mid==null) {
			Newsgroup group = (Newsgroup)article.getFolder();
			if (!current.equals(group)) open(group);
			mid = Integer.toString(article.getMessageNumber());
		}
		try {
			send("BODY "+mid);
			switch (getResponse()) {
			  case ARTICLE_RETRIEVED_BODY:
				int max = fetchsize, len;
				byte b[] = new byte[max];
				MessageInputStream min = new MessageInputStream(in);
				ByteArrayOutputStream bout = new ByteArrayOutputStream();
				while ((len = min.read(b, 0, max))!=-1)
					bout.write(b, 0, len);
				return bout.toByteArray();
			  case NO_GROUP_SELECTED:
				throw new MessagingException("No group selected");
			  case NO_ARTICLE_SELECTED:
				throw new MessagingException("No article selected");
			  case NO_SUCH_ARTICLE_IN_GROUP:
				throw new MessagingException("No such article in group");
			  case NO_SUCH_ARTICLE:
				throw new MessagingException("No such article");
			  case SERVER_ERROR:
				if (response.toLowerCase().indexOf("timeout")>-1) {
					close();
					connect();
					return getContent(article);
				}
			  default:
				throw new MessagingException("unexpected server response: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

	/**
	 * Post an article.
	 * @param article the article
	 * @param addresses the addresses to post to.
	 * @exception MessagingException if a messaging exception occurred or there were no newsgroup recipients
	 */
	public void postArticle(Message article, Address[] addresses) throws MessagingException {
		Vector v = new Vector();
		for (int i=0; i<addresses.length; i++) { // get unique newsgroup addresses
			if (addresses[i] instanceof NewsAddress && !v.contains(addresses[i]))
				v.addElement(addresses[i]);
		}
		NewsAddress[] a = new NewsAddress[v.size()]; v.copyInto(a);
		if (a.length==0) throw new MessagingException("No newsgroups specified as recipients");
		for (int i=0; i<a.length; i++)
			post(article, a[i]);
	}

	/**
	 * Returns a Transport that can be used to send articles to this news server.
	 */
	public Transport getTransport() {
		return this.new NNTPTransport(session, url);
	}

    // Posts an article to the specified newsgroup.
	synchronized void post(Message article, NewsAddress address) throws MessagingException {
		String group = address.getNewsgroup();
		try {
			send("POST");
			switch (getResponse()) {
			  case SEND_ARTICLE:
				MessageOutputStream mout = new MessageOutputStream(out);
				article.writeTo(mout);
				out.write("\n.\n".getBytes());
				out.flush();
				switch (getResponse()) {
				 case ARTICLE_POSTED:
					break;
				 case POSTING_FAILED:
					throw new MessagingException("Posting failed: "+response);
				 default:
					throw new MessagingException(response);
				}
				break;
			  case POSTING_NOT_ALLOWED:
				throw new MessagingException("Posting not allowed");
			  case POSTING_FAILED:
				throw new MessagingException("Posting failed");
			  case SERVER_ERROR:
				if (response.toLowerCase().indexOf("timeout")>-1) {
					connect();
					post(article, address);
					break;
				}
			  default:
				throw new MessagingException("unexpected server response: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("I/O error", e);
		}
	}

	// Attempts to discover which newsgroups exist and which articles have been read.
	void readNewsrc() {
		try {
		  File newsrc;
      String osname = System.getProperties().getProperty("os.name");
      if (osname.startsWith("Windows") || osname.startsWith("Win32") ||
          osname.startsWith("Win16") || osname.startsWith("16-bit Windows")) {
  			newsrc = new File(System.getProperty("user.home")+File.separator+"news-"+getHostName()+".rc");
  			if (!newsrc.exists())
  				newsrc = new File(System.getProperty("user.home")+File.separator+"news.rc");
  	  } else {
  			newsrc = new File(System.getProperty("user.home")+File.separator+".newsrc-"+getHostName());
  			if (!newsrc.exists())
  				newsrc = new File(System.getProperty("user.home")+File.separator+".newsrc");
      }
			BufferedReader reader = new BufferedReader(new FileReader(newsrc));
			String line;
			while ((line = reader.readLine())!=null) {
				StringTokenizer st = new StringTokenizer(line, ":!", true);
				try {
					String name = st.nextToken();
					Newsgroup group = new Newsgroup(this, name);
					group.subscribed = ":".equals(st.nextToken());
					if (st.hasMoreTokens())
						group.newsrcline = st.nextToken().trim();
					newsgroups.put(name, group);
				} catch (NoSuchElementException e) {}
			}
		} catch (FileNotFoundException e) {
		} catch (IOException e) {
		} catch (SecurityException e) { // not allowed to read file
		}
	}

	// Returns the newsgroups available in this store via the specified listing command.
	Newsgroup[] getNewsgroups(String command, boolean retry) throws MessagingException {
        Vector vector = new Vector();
		try {
			send(command);
			switch (getResponse()) {
			  case LISTING:
				String line;
				for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
					StringTokenizer st = new StringTokenizer(line, " ");
					String name = st.nextToken();
					int last = Integer.parseInt(st.nextToken());
					int first = Integer.parseInt(st.nextToken());
					boolean posting = ("y".equals(st.nextToken().toLowerCase()));
					Newsgroup group = (Newsgroup)getFolder(name);
					group.first = first;
					group.last = last;
					group.postingAllowed = posting;
					vector.addElement(group);
				}
				break;
			  case SERVER_ERROR:
				if (!retry) {
					if (response.toLowerCase().indexOf("timeout")>-1) {
						close();
						connect();
						return getNewsgroups(command, true);
					} else
						return getNewsgroups("LIST", false); // backward compatibility with rfc977
				}
			  default:
				throw new MessagingException(command+" failed: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException(command+" failed", e);
		} catch (NumberFormatException e) {
			throw new MessagingException(command+" failed", e);
		}
		Newsgroup[] groups = new Newsgroup[vector.size()]; vector.copyInto(groups);
		return groups;
	}

	// Returns the articles for a newsgroup.
	Article[] getArticles(Newsgroup newsgroup) throws MessagingException {
		try {
			return getOverview(newsgroup);
		} catch (MessagingException e) { // try rfc977
			return getNewArticles(newsgroup, new Date(0L));
		}
	}

	/**
	 * Returns the newsgroups added to this store since the specified date.
     * @exception MessagingException if a messaging error occurred
	 */
	Newsgroup[] getNewFolders(Date date) throws MessagingException {
        String datetime = getDateTimeString(date);
		Vector vector = new Vector();
		try {
			send("NEWGROUPS "+datetime);
			switch (getResponse()) {
			  case LISTING_NEW:
				String line;
				for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
					StringTokenizer st = new StringTokenizer(line, " ");
					String name = st.nextToken();
					int last = Integer.parseInt(st.nextToken());
					int first = Integer.parseInt(st.nextToken());
					boolean posting = ("y".equals(st.nextToken().toLowerCase()));
					Newsgroup group = (Newsgroup)getFolder(name);
					group.first = first;
					group.last = last;
					group.postingAllowed = posting;
					vector.addElement(group);
				}
				break;
			  default:
				throw new MessagingException("Listing failed: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException("Listing failed", e);
		} catch (NumberFormatException e) {
			throw new MessagingException("Listing failed", e);
		}
		Newsgroup[] groups = new Newsgroup[vector.size()]; vector.copyInto(groups);
		return groups;
	}

    // Returns the articles added to the specified newsgroup since the specified date.
	Article[] getNewArticles(Newsgroup newsgroup, Date date) throws MessagingException {
        String command = "NEWNEWS "+newsgroup.getName()+" "+getDateTimeString(date);
		Vector vector = new Vector();
		try {
			send(command);
			switch (getResponse()) {
			  case LISTING_ARTICLES:
				String line;
				for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
					Message article = getArticle(newsgroup, line);
					vector.addElement(article);
				}
				break;
			  default:
				throw new MessagingException(command+" failed: "+response);
			}
		} catch (IOException e) {
			throw new MessagingException(command+" failed", e);
		} catch (NumberFormatException e) {
			throw new MessagingException(command+" failed", e);
		}
		Article[] articles = new Article[vector.size()]; vector.copyInto(articles);
		return articles;
	}

	// Returns a GMT date-time formatted string for the specified date,
	// suitable as an argument to NEWGROUPS and NEWNEWS commands.
	String getDateTimeString(Date date) {
		Calendar calendar = Calendar.getInstance();
		calendar.setTimeZone(TimeZone.getTimeZone("GMT"));
		calendar.setTime(date);
		StringBuffer buffer = new StringBuffer();
		int field;
		String ZERO = "0"; // leading zero
		field = calendar.get(Calendar.YEAR)%100; buffer.append((field<10) ? ZERO+field : Integer.toString(field));
		field = calendar.get(Calendar.MONTH)+1; buffer.append((field<10) ? ZERO+field : Integer.toString(field));
		field = calendar.get(Calendar.DAY_OF_MONTH); buffer.append((field<10) ? ZERO+field : Integer.toString(field));
        buffer.append(" ");
		field = calendar.get(Calendar.HOUR_OF_DAY); buffer.append((field<10) ? ZERO+field : Integer.toString(field));
		field = calendar.get(Calendar.MINUTE); buffer.append((field<10) ? ZERO+field : Integer.toString(field));
		field = calendar.get(Calendar.SECOND); buffer.append((field<10) ? ZERO+field : Integer.toString(field));
		buffer.append(" GMT");
		return buffer.toString();
	}

	/**
	 * Returns the root folder.
	 */
	public Folder getDefaultFolder() throws MessagingException {
		synchronized (this) {
			if (root==null) root = new Root(this);
		}
		return root;
	}

	/**
	 * Returns the newsgroup with the specified name.
	 */
	public Folder getFolder(String name) throws MessagingException {
		return getNewsgroup(name);
	}

	/**
	 * Returns the newsgroup specified as part of a URLName.
	 */
	public Folder getFolder(URLName urlname) throws MessagingException {
		String group = urlname.getFile();
		int hashIndex = group.indexOf('#');
        if (hashIndex>-1) group = group.substring(0, hashIndex);
		return getNewsgroup(group);
	}

	Newsgroup getNewsgroup(String name) {
		Newsgroup newsgroup = (Newsgroup)newsgroups.get(name);
		if (newsgroup==null) {
			newsgroup = new Newsgroup(this, name);
			newsgroups.put(name, newsgroup);
		}
		return newsgroup;
	}

	// Returns the article with the specified message-id for the newsgroup.
	Article getArticle(Newsgroup newsgroup, String mid) throws MessagingException {
		Article article = (Article)articles.get(mid);
		if (article==null) {
			article = new Article(newsgroup, mid);
			articles.put(mid, article);
		}
		return article;
	}

	// -- NNTP extensions --

	int[] getArticleNumbers(Newsgroup newsgroup) throws MessagingException {
		String command = "LISTGROUP "+newsgroup.getName();
		Vector vector = new Vector();
		synchronized (this) {
			try {
				send(command);
				switch (getResponse()) {
				  case GROUP_SELECTED:
					String line;
					for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
						vector.addElement(line);
					}
					break;
				  case NO_GROUP_SELECTED:
				  case PERMISSION_DENIED:
				  default:
					throw new MessagingException(command+" failed: "+response);
				}
			} catch (IOException e) {
				throw new MessagingException(command+" failed", e);
			} catch (NumberFormatException e) {
				throw new MessagingException(command+" failed", e);
			}
		}
		int[] numbers = new int[vector.size()];
		for (int i=0; i<numbers.length; i++)
			numbers[i] = Integer.parseInt((String)vector.elementAt(i));
		return numbers;
	}

	String[] overviewFormat;

	String[] getOverviewFormat() throws MessagingException {
        if (overviewFormat!=null) return overviewFormat;
        String command = "LIST OVERVIEW.FMT";
		synchronized (this) {
			Vector vector = new Vector();
			try {
				send(command);
				switch (getResponse()) {
				  case LISTING:
					String line;
					for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
						vector.addElement(line);
					}
					break;
				  case SERVER_ERROR:
				  default:
					throw new MessagingException(command+" failed: "+response);
				}
			} catch (IOException e) {
				throw new MessagingException(command+" failed", e);
			} catch (NumberFormatException e) {
				throw new MessagingException(command+" failed", e);
			}
			overviewFormat = new String[vector.size()]; vector.copyInto(overviewFormat);
			return overviewFormat;
		}
	}

    // Prefetches header information for the specified messages, if possible
	Article[] getOverview(Newsgroup newsgroup) throws MessagingException {
        String name = newsgroup.getName();
        String[] format = getOverviewFormat();
		String command = "GROUP "+name;
		if (current==null || !name.equals(current.getName())) { // select the group
			synchronized (this) {
				try {
					send(command);
					switch (getResponse()) {
					  case GROUP_SELECTED:
						try {
							updateGroup(newsgroup, response);
							current = newsgroup;
						} catch(NumberFormatException e) {
							throw new MessagingException("NNTP protocol exception: "+response, e);
						}
					  case NO_SUCH_GROUP:
						throw new MessagingException("No such group");
					  case SERVER_ERROR:
						if (response.toLowerCase().indexOf("timeout")>-1) {
							close();
							connect();
							return getOverview(newsgroup);
						}
					  default:
						throw new MessagingException(command+" failed: "+response);
					}
				} catch (IOException e) {
					throw new MessagingException(command+" failed", e);
				}
			}
		}
		command = "XOVER "+newsgroup.first+"-"+newsgroup.last;
		Vector av = new Vector(Math.max(newsgroup.last-newsgroup.first, 10));
		synchronized (this) {
			try {
				send(command);
				switch (getResponse()) {
				  case LISTING_OVERVIEW:
					String line;
					for (line=in.readLine(); line!=null && !".".equals(line); line = in.readLine()) {
						int tabIndex = line.indexOf('\t');
						if (tabIndex>-1) {
							int msgnum = Integer.parseInt(line.substring(0, tabIndex));
                            Article article = new Article(newsgroup, msgnum);
							article.addXoverHeaders(getOverviewHeaders(format, line, tabIndex));
							av.addElement(article);
						} else
							throw new ProtocolException("Invalid overview line format");
					}
					break;
				  case NO_ARTICLE_SELECTED:
				  case PERMISSION_DENIED:
					break;
				  case NO_GROUP_SELECTED:
				  case SERVER_ERROR:
				  default:
					throw new MessagingException(command+" failed: "+response);
				}
			} catch (IOException e) {
				throw new MessagingException(command+" failed", e);
			} catch (NumberFormatException e) {
				throw new MessagingException(command+" failed", e);
			}
		}
		Article[] articles = new Article[av.size()]; av.copyInto(articles);
		return articles;
	}

    // Returns an InternetHeaders object representing the headers stored in an xover response line.
	InternetHeaders getOverviewHeaders(String[] format, String line, int startIndex) {
        InternetHeaders headers = new InternetHeaders();
		for (int i=0; i<format.length; i++) {
			//System.out.println("Processing fmt key: "+format[i]);
			int colonIndex = format[i].indexOf(':');
			String key = format[i].substring(0, colonIndex);
			boolean full = "full".equals(format[i].substring(colonIndex+1, format[i].length()));
			int tabIndex = line.indexOf('\t', startIndex+1);
			if (tabIndex<0) tabIndex = line.length();
			String value = line.substring(startIndex+1, tabIndex);
			if (full)
				value = value.substring(value.indexOf(':')+1).trim();
			headers.addHeader(key, value);
			//System.out.println("Added header: "+key+", "+value);
			startIndex = tabIndex;
		}
		return headers;
	}

	boolean validateOverviewHeader(String key) throws MessagingException {
		String[] format = getOverviewFormat();
		for (int i=0; i<format.length; i++) {
			if (key.equalsIgnoreCase(format[i].substring(0, format[i].indexOf(':'))))
				return true;
		}
		return false;
	}

	public void addStatusListener(StatusListener l) {
		synchronized (statusListeners) {
			statusListeners.addElement(l);
		}
	}

	public void removeStatusListener(StatusListener l) {
		synchronized (statusListeners) {
			statusListeners.removeElement(l);
		}
	}

	protected void processStatusEvent(StatusEvent event) {
        StatusListener[] listeners;
		synchronized (statusListeners) {
			listeners = new StatusListener[statusListeners.size()];
			statusListeners.copyInto(listeners);
		}
		switch (event.getType()) {
		  case StatusEvent.OPERATION_START:
			for (int i=0; i<listeners.length; i++)
				listeners[i].statusOperationStarted(event);
			break;
		  case StatusEvent.OPERATION_UPDATE:
			for (int i=0; i<listeners.length; i++)
				listeners[i].statusProgressUpdate(event);
			break;
		  case StatusEvent.OPERATION_END:
			for (int i=0; i<listeners.length; i++)
				listeners[i].statusOperationEnded(event);
            break;
		}
	}

	/**
	 * The root holds the newsgroups in an NNTPStore.
	 */
	class Root extends Folder {

		/**
		 * Constructor.
		 */
		protected Root(Store store) { super(store); }

		/**
		 * Returns the name of this folder.
		 */
		public String getName() { return "/"; }

		/**
		 * Returns the full name of this folder.
		 */
		public String getFullName() { return getName(); }

		/**
		 * Returns the type of this folder.
		 */
		public int getType() throws MessagingException { return HOLDS_FOLDERS; }

		/**
		 * Indicates whether this folder exists.
		 */
		public boolean exists() throws MessagingException { return true; }

		/**
		 * Indicates whether this folder contains any new articles.
		 */
		public boolean hasNewMessages() throws MessagingException { return false; }

		/**
		 * Opens this folder.
		 */
		public void open(int mode) throws MessagingException {
			if (mode!=this.READ_ONLY) throw new MessagingException("Folder is read-only");
		}

		/**
		 * Closes this folder.
		 */
		public void close(boolean expunge) throws MessagingException {}

		/**
		 * Expunges this folder.
		 */
		public Message[] expunge() throws MessagingException { return new Message[0]; }

		/**
		 * Indicates whether this folder is open.
		 */
		public boolean isOpen() { return true; }

		/**
		 * Returns the permanent flags for this folder.
		 */
		public Flags getPermanentFlags() { return new Flags(); }

		/**
		 * Returns the number of articles in this folder.
		 */
		public int getMessageCount() throws MessagingException { return 0; }

		/**
		 * Returns the articles in this folder.
		 */
		public Message[] getMessages() throws MessagingException {
			throw new MessagingException("Folder can't contain messages");
		}

		/**
		 * Returns the specified message in this folder.
		 * Since NNTP articles are not stored in sequential order,
		 * the effect is just to reference articles returned by getMessages().
		 */
		public Message getMessage(int msgnum) throws MessagingException {
			throw new MessagingException("Folder can't contain messages");
		}

		/**
		 * Root folder is read-only.
		 */
		public void appendMessages(Message aarticle[]) throws MessagingException {
			throw new MessagingException("Folder is read-only");
		}

		/**
		 * Does nothing.
		 */
		public void fetch(Message articles[], FetchProfile fetchprofile) throws MessagingException {
		}

		/**
		 * This folder does not have a parent.
		 */
		public Folder getParent() throws MessagingException { return null; }

		/**
		 * Returns the newsgroups on the server.
		 */
		public Folder[] list() throws MessagingException {
			return getNewsgroups("LIST ACTIVE", false);
		}

		/**
		 * Returns the newsgroups on the server.
		 */
		public Folder[] list(String pattern) throws MessagingException {
			return getNewsgroups(pattern, false);
		}

		/**
		 * Returns the subscribed newsgroups on the server.
		 */
		public Folder[] listSubscribed() throws MessagingException {
			Vector groups = new Vector();
			for (Enumeration enumer = newsgroups.elements(); enumer.hasMoreElements(); ) {
				Newsgroup group = (Newsgroup)enumer.nextElement();
				if (group.subscribed)
					groups.addElement(group);
			}
			Folder[] list = new Folder[groups.size()]; groups.copyInto(list);
			return list;
		}

		/**
		 * Returns the subscribed newsgroups on the server.
		 */
		public Folder[] listSubscribed(String pattern) throws MessagingException {
			return listSubscribed();
		}

		/**
		 * Returns the newsgroup with the specified name.
		 */
		public Folder getFolder(String name) throws MessagingException {
			return getNewsgroup(name);
		}

		/**
		 * Returns the separator character.
		 */
		public char getSeparator() throws MessagingException {
			return '.';
		}

		/**
		 * Root folders cannot be created, deleted, or renamed.
		 */
		public boolean create(int i) throws MessagingException {
			throw new MessagingException("Folder cannot be created");
		}

		/**
		 * Root folders cannot be created, deleted, or renamed.
		 */
		public boolean delete(boolean flag) throws MessagingException {
			throw new MessagingException("Folder cannot be deleted");
		}

		/**
		 * Root folders cannot be created, deleted, or renamed.
		 */
		public boolean renameTo(Folder folder) throws MessagingException {
			throw new MessagingException("Folder cannot be renamed");
		}

	}

	class NNTPTransport extends Transport {

		NNTPTransport(Session session, URLName urlname) {
			super(session, urlname);
		}

		public boolean protocolConnect(String host, int port, String user, String password) throws MessagingException {
			return true;
		}

		public void sendMessage(Message message, Address[] addresses) throws MessagingException {
			postArticle(message, addresses);
		}

	}

}
