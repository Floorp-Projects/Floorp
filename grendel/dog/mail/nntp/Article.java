/*
 * Article.java
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
 * Contributor(s): n/a.
 */

package dog.mail.nntp;

import java.io.*;
import java.util.*;
import javax.activation.DataHandler;
import javax.mail.*;
import javax.mail.internet.*;

/**
 * The message class implementing the NNTP mail protocol.
 *
 * @author dog@dog.net.uk
 * @version 1.0
 */
public class Article extends MimeMessage {

	/**
	 * The unique message-id of this message.
	 */
	protected String messageId;

    // Whether the headers for this article were retrieved using xover.
	boolean xoverHeaders = false;

	/**
	 * Creates an NNTP message with the specified article number.
	 */
	protected Article(Newsgroup folder, int msgnum) throws MessagingException {
		super(folder, msgnum);
		checkNewsrc(folder.newsrcline);
	}

	/**
	 * Creates an NNTP message with the specified message-id.
	 */
	protected Article(Newsgroup folder, String messageId) throws MessagingException {
		super(folder, 0);
		this.messageId = messageId;
		checkNewsrc(folder.newsrcline);
	}

    // Adds headers retrieved by an xover call to this article.
	void addXoverHeaders(InternetHeaders headers) {
		if (headers==null) {
			this.headers = headers;
			xoverHeaders = true;
		}
	}
	
	void checkNewsrc(String newsrcline) throws MessagingException {
		if (newsrcline!=null) {
			int articlenum = getArticleNumber();
			StringTokenizer st = new StringTokenizer(newsrcline, ",");
			while (st.hasMoreTokens()) {
				String token = st.nextToken();
				int hyphenIndex = token.indexOf('-');
				if (hyphenIndex>-1) { // range
					try {
						int lo = Integer.parseInt(token.substring(0, hyphenIndex));
						int hi = Integer.parseInt(token.substring(hyphenIndex+1));
						if (articlenum>=lo && articlenum<=hi) {
							setFlag(Flags.Flag.SEEN, true);
							return;
						}
					} catch (NumberFormatException e) {
					}
				} else { // single number
					try {
						int num = Integer.parseInt(token);
						if (articlenum==num) {
							setFlag(Flags.Flag.SEEN, true);
							return;
						}
					} catch (NumberFormatException e) {
					}
				}
			}
		}
	}
	
	int getArticleNumber() {
		return getMessageNumber();
	}

	/**
	 * Returns the message content.
	 */
	public Object getContent() throws IOException, MessagingException {
		if (content==null) content = ((NNTPStore)folder.getStore()).getContent(this);
		
		/*
		// check for embedded uuencoded content
		BufferedReader reader = new BufferedReader(new InputStreamReader(ByteArrayInputStream(content)));
		Hashtable parts = null;
		try {
			StringBuffer buffer = new StringBuffer();
			int count = 1;
			for (String line = reader.readLine(); line!=null; line = reader.readLine()) {
				if (line.startsWith("begin")) {
					StringTokenizer st = new StringTokenizer(line, " ");
					if (st.countTokens()==3) {
						String ignore = st.nextToken();
						int lines = Integer.parseInt(st.nextToken());
						String name = st.nextToken();
						String contentType = guessContentType(name, count++);
						if (contentType!=null) {
							parts = new Hashtable();
							if (buffer.length>0) {
								parts.put("text/plain; partid="+(count++), buffer.toString());
								buffer = new StringBuffer();
							}
							ByteArrayOutputStream bout = new ByteArrayOutputStream();
							
						}						
					}
				} else
					buffer.append(line);
			}
		} catch (NumberFormatException e) {
		} finally {
			reader.close();
		}
		 */
		return super.getContent();
	}
	
	String guessContentType(String name, int count) {
		String lname = name.toLowerCase();
		if (lname.indexOf(".jpg")>-1 || lname.indexOf(".jpeg")>-1)
			return "image/jpeg; name=\""+name+"\", partid="+count;
		if (lname.indexOf(".gif")>-1)
			return "image/gif; name=\""+name+"\", partid="+count;
		if (lname.indexOf(".au")>-1)
			return "audio/basic; name=\""+name+"\", partid="+count;
		return null;
	}

	/**
	 * Returns the from address.
	 */
	public Address[] getFrom() throws MessagingException {
        NNTPStore s = (NNTPStore)folder.getStore();
		if (xoverHeaders && !s.validateOverviewHeader("From")) headers = null;
		if (headers==null) headers = s.getHeaders(this);

		Address[] a = getAddressHeader("From");
		if (a==null) a = getAddressHeader("Sender");
		return a;
	}

	/**
	 * Returns the recipients' addresses for the specified RecipientType.
	 */
	public Address[] getRecipients(RecipientType type) throws MessagingException {
		NNTPStore s = (NNTPStore)folder.getStore();
		if (xoverHeaders && ! s.validateOverviewHeader(getHeaderKey(type))) headers = null;
		if (headers==null) headers = s.getHeaders(this);
		
		if (type==RecipientType.NEWSGROUPS) {
			String key = getHeader("Newsgroups", ",");
			if (key==null) return null;
			return NewsAddress.parse(key);
		} else {
			return getAddressHeader(getHeaderKey(type));
		}
	}

	/**
	 * Returns all the recipients' addresses.
	 */
	public Address[] getAllRecipients() throws MessagingException {
		if (headers==null || xoverHeaders) headers = ((NNTPStore)folder.getStore()).getHeaders(this);
		
		return super.getAllRecipients();
	}

	/**
	 * Returns the reply-to address.
	 */
	public Address[] getReplyTo() throws MessagingException {
        NNTPStore s = (NNTPStore)folder.getStore();
		if (xoverHeaders && !s.validateOverviewHeader("Reply-To")) headers = null;
		if (headers==null) headers = s.getHeaders(this);

		Address[] a = getAddressHeader("Reply-To");
		if (a==null) a = getFrom();
		return a;
	}

	/**
	 * Returns the subject line.
	 */
	public String getSubject() throws MessagingException {
        NNTPStore s = (NNTPStore)folder.getStore();
		if (xoverHeaders && !s.validateOverviewHeader("Subject")) headers = null;
		if (headers==null) headers = s.getHeaders(this);

		return super.getSubject();
	}

	/**
	 * Returns the sent date.
	 */
	public Date getSentDate() throws MessagingException {
        NNTPStore s = (NNTPStore)folder.getStore();
		if (xoverHeaders && !s.validateOverviewHeader("Date")) headers = null;
		if (headers==null) headers = s.getHeaders(this);

		return super.getSentDate();
	}

	/**
	 * Returns the received date.
	 */
	public Date getReceivedDate() throws MessagingException {
		if (headers==null || xoverHeaders) headers = ((NNTPStore)folder.getStore()).getHeaders(this);
		
		return super.getReceivedDate();
	}

	/**
	 * Returns an array of addresses for the specified header key.
	 */
	protected Address[] getAddressHeader(String key) throws MessagingException {
		String header = getHeader(key, ",");
		if (header==null) return null;
		try {
			return InternetAddress.parse(header);
		} catch (AddressException e) {
            String message = e.getMessage();
			if (message!=null && message.indexOf("@domain")>-1)
				try {
					return parseAddress(header, ((NNTPStore)folder.getStore()).getHostName());
				} catch (AddressException e2) {
					throw new MessagingException("Invalid address: "+header, e);
				}
			throw e;
		}
	}

	/**
	 * Makes a pass at parsing internet addresses.
	 */
	protected Address[] parseAddress(String in, String defhost) throws AddressException {
        Vector v = new Vector();
		for (StringTokenizer st = new StringTokenizer(in, ","); st.hasMoreTokens(); ) {
            String s = st.nextToken().trim();
			try {
				v.addElement(new InternetAddress(s));
			} catch (AddressException e) {
				int index = s.indexOf('>');
				if (index>-1) { // name <address>
					StringBuffer buffer = new StringBuffer();
					buffer.append(s.substring(0, index));
					buffer.append('@');
					buffer.append(defhost);
					buffer.append(s.substring(index));
					v.addElement(new InternetAddress(buffer.toString()));
				} else {
					index = s.indexOf(" (");
					if (index>-1) { // address (name)
						StringBuffer buffer = new StringBuffer();
						buffer.append(s.substring(0, index));
						buffer.append('@');
						buffer.append(defhost);
						buffer.append(s.substring(index));
						v.addElement(new InternetAddress(buffer.toString()));
					} else // address
						v.addElement(new InternetAddress(s+"@"+defhost));
				}

			}
		}
        Address[] a = new Address[v.size()]; v.copyInto(a);
		return a;
	}

	/**
	 * Returns the header key for the specified RecipientType.
	 */
	protected String getHeaderKey(RecipientType type) throws MessagingException {
		if (type==RecipientType.TO)
			return "To";
		if (type==RecipientType.CC)
			return "Cc";
		if (type==RecipientType.BCC)
			return "Bcc";
		if (type==RecipientType.NEWSGROUPS)
			return "Newsgroups";
		throw new MessagingException("Invalid recipient type: "+type);
	}

	// -- Need to override these since we are read-only --

	/**
	 * NNTP messages are read-only.
	 */
	public void setFrom(Address address) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void addFrom(Address a[]) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setRecipients(javax.mail.Message.RecipientType recipienttype, Address a[]) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void addRecipients(javax.mail.Message.RecipientType recipienttype, Address a[]) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setReplyTo(Address a[]) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setSubject(String s, String s1) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setSentDate(Date date) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setDisposition(String s) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setContentID(String s) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setContentMD5(String s) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setDescription(String s, String s1) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

	/**
	 * NNTP messages are read-only.
	 */
	public void setDataHandler(DataHandler datahandler) throws MessagingException {
		throw new IllegalWriteException("Article is read-only");
	}

}
