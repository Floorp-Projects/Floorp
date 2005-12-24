/*
 * Message.java
 *
 * Created on 18 August 2005, 23:46
 *
 * To change this template, choose Tools | Options and locate the template under
 * the Source Creation and Management node. Right-click the template and choose
 * Open. You can then make changes to the template in the Source Editor.
 */
package grendel.structure;

import calypso.util.ByteBuf;

import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;

import grendel.prefs.accounts.Identity;

import grendel.renderer.Renderer;

import grendel.storage.addressparser.AddressList;
import grendel.storage.addressparser.RFC822MailboxList;

import grendel.structure.events.Event;
import grendel.structure.events.message.FlagChangedEvent;
import grendel.structure.events.message.MessageDeletedEvent;
import grendel.structure.events.message.MessageEvent;
import grendel.structure.events.message.MessageListener;
import grendel.structure.events.message.MessageMovedEvent;

import grendel.structure.sending.NewMessage;

import grendel.util.Constants;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.Enumeration;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.Vector;

import javax.mail.Address;
import javax.mail.BodyPart;
import javax.mail.Flags;
import javax.mail.Flags.Flag;
import javax.mail.Header;
import javax.mail.MessagingException;
import javax.mail.internet.InternetAddress;
import javax.mail.internet.InternetHeaders;
import javax.mail.internet.MimeMultipart;
import javax.mail.internet.MimeUtility;


/**
 * This class is used instead of MessageExtra.
 * @author hash9
 */
public class Message
{
    protected javax.mail.Message message;
    private Folder folder;
    private boolean deleted = false;
    protected Server server;
    protected EventDispatcher dispatcher = new EventDispatcher();

    /** Creates a new instance of Message */
    protected Message(Folder folder, javax.mail.Message message)
    {
        this.message = message;
        this.folder = folder;
    }

    public boolean copyTo(Folder f)
    {
        try
        {
            f.folder.appendMessages(new javax.mail.Message[] { message });

            return true;
        }
        catch (MessagingException e)
        {
            NoticeBoard.publish(new ExceptionNotice(e));

            return true;
        }

        //throw new UnsupportedOperationException();
    }

    public boolean moveTo(Folder f)
    {
        if (copyTo(f))
        {
            dispatcher.dispatch(new MessageMovedEvent(this));
            delete();

            return true;
        }

        return false;
    }

    public final Folder getFolder()
    {
        return folder;
    }

    public boolean isImportant()
    {
        return getFlag(Flag.FLAGGED);
    }

    public void setMarkAsDeleted(boolean value)
    {
        setFlag(Flag.DELETED, value);
    }

    public void setMarkAsImportant(boolean value)
    {
        setFlag(Flag.FLAGGED, value);
    }

    public void setMarkAsRead(boolean value)
    {
        setFlag(Flag.SEEN, value);
    }

    public void setMarkAsReplied(boolean value)
    {
        setFlag(Flag.ANSWERED, value);
    }

    public boolean isRead()
    {
        return getFlag(Flag.SEEN);
    }

    public boolean isReplied()
    {
        return getFlag(Flags.Flag.ANSWERED);
    }

    public AddressList getAllRecipients() throws javax.mail.MessagingException
    {
        AddressList to = getRecipients();
        AddressList cc = getRecipientsCC();
        AddressList bcc = getRecipientsBCC();

        AddressList list = new AddressList();
        list.addAll(to);
        list.addAll(cc);
        list.addAll(bcc);

        return list;
    }

    public InternetAddress getAuthor()
    {
        return getAddress("From");
    }

    public boolean isForwarded()
    {
        return getFlags(new Flags("Forwarded"));

        // #### this flags crap is all messed up, since Sun munged the APIs.
        // throw new UnsupportedOperationException();
    }

    public void setMarkAsForwarded(boolean value)
    {
        setFlags(new Flags("Forwarded"), value);

        // #### this flags crap is all messed up, since Sun munged the APIs.
        // throw new UnsupportedOperationException();
    }

    public String getMessageID()
    {
        return getHeader("Message-ID");
    }

    public int getMessageNumber()
    {
        throw new UnsupportedOperationException();
    }

    public Date getReceivedDate() throws MessagingException
    {
        return message.getReceivedDate();
    }

    public Address getRecipient()
    {
        return getAddress("To");
    }

    public AddressList getRecipients()
    {
        return getAddresses("To");
    }

    public AddressList getRecipientsCC()
    {
        return getAddresses("CC");
    }

    public AddressList getRecipientsBCC()
    {
        return getAddresses("BCC");
    }

    public AddressList getReplyTo() throws MessagingException
    {
        return getAddresses("Reply-To");
    }

    public Date getSentDate() throws MessagingException
    {
        return message.getSentDate();
    }

    public int getSize() throws MessagingException
    {
        return message.getSize();
    }

    public String getSubject() throws MessagingException
    {
        return message.getSubject();
    }

    /*public javax.mail.Message reply(boolean b) throws javax.mail.MessagingException {
        return message.reply(b);
    }*/
    public void saveChanges() throws javax.mail.MessagingException
    {
        message.saveChanges();
    }

    protected AddressList getAddresses(String headername)
    {
        String header = getHeader(headername);

        if (header == null)
        {
            return new AddressList();
        }

        //XXX THIS WILL PRODUCE A BUG!!!! 
        // The RFC822 Parser (by definition) has no support for news message
        // addresses this WILL have to be addressed at some point
        return new AddressList(new RFC822MailboxList(header));
    }

    protected InternetAddress getAddress(String headername)
    {
        AddressList boxes = getAddresses(headername);

        if (boxes.size() < 1)
        {
            return null;
        }

        return (InternetAddress) boxes.get(0);
    }

    public boolean isAddressee()
    {
        Collection<Identity> identies = getServer().getAccount()
                                            .getCollectionIdentities();
        AddressList recipients = getRecipients();

        for (Address address : recipients)
        {
            for (Identity ident : identies)
            {
                try
                {
                    Address ident_address = new InternetAddress(ident.getName(),
                            ident.getEMail());

                    if (ident_address.equals(recipients))
                    {
                        return true;
                    }
                }
                catch (UnsupportedEncodingException ex)
                {
                }
            }
        }

        return false;
    }

    public String baseSubject()
    {
        try
        {
            String sub = getSubject();

            if (sub != null)
            {
                StringBuilder buf = new StringBuilder(sub);

                if (stripRe(buf))
                {
                    return buf.toString();
                }

                return sub;
            }
        }
        catch (MessagingException e)
        {
            NoticeBoard.publish(new ExceptionNotice(e));
        }

        return "";
    }

    public boolean isForward()
    {
        try
        {
            return getSubject().toLowerCase().contains("fwd:");
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));

            return false;
        }
    }

    public boolean isReply()
    {
        try
        {
            String sub = message.getSubject();

            if (sub != null)
            {
                StringBuilder buf = new StringBuilder(sub);

                return stripRe(buf);
            }
        }
        catch (MessagingException e)
        {
            NoticeBoard.publish(new ExceptionNotice(e));
        }

        return false;
    }

    // Removes leading "Re:" or similar from the given StringBuffer.  Returns
    // true if it found such a string to remove; false otherwise.
    protected boolean stripRe(StringBuilder buf)
    {
        // Much of this code is duplicated in MessageBase.  Sigh. ###
        if (buf == null)
        {
            return false;
        }

        int numToTrim = 0;
        int length = buf.length();

        if ((length > 2) && ((buf.charAt(0) == 'r') || (buf.charAt(0) == 'R')) &&
                ((buf.charAt(1) == 'e') || (buf.charAt(1) == 'E')))
        {
            char c = buf.charAt(2);

            if (c == ':')
            {
                numToTrim = 3; // Skip over "Re:"
            }
            else if ((c == '[') || (c == '('))
            {
                int i = 3; // skip over "Re[" or "Re("

                while ((i < length) && (buf.charAt(i) >= '0') &&
                        (buf.charAt(i) <= '9'))
                {
                    i++;
                }

                // Now ensure that the following thing is "]:" or "):"
                // Only if it is do we treat this all as a "Re"-ish thing.
                if ((i < (length - 1)) &&
                        ((buf.charAt(i) == ']') || (buf.charAt(i) == ')')) &&
                        (buf.charAt(i + 1) == ':'))
                {
                    numToTrim = i + 2; // Skip the whole thing.
                }
            }
        }

        if (numToTrim > 0)
        {
            int i = numToTrim;

            while ((i < (length - 1)) && Character.isWhitespace(buf.charAt(i)))
            {
                i++;
            }

            for (int j = i; j < length; j++)
            {
                buf.setCharAt(j - i, buf.charAt(j));
            }

            buf.setLength(length - i);

            return true;
        }

        return false;
    }

    public boolean equals(Object o)
    {
        if (o instanceof javax.mail.Message)
        {
            return equals((javax.mail.Message) o);
        }
        else if (o instanceof Message)
        {
            return equals((Message) o);
        }

        return super.equals(o);
    }

    public int hashcode()
    {
        return getMessageID().hashCode();
    }

    public boolean equals(javax.mail.Message message)
    {
        String message_id_this = this.getMessageID();
        String[] list = null;

        try
        {
            list = message.getHeader("Message-ID");
        }
        catch (MessagingException e)
        {
        }

        if ((list == null) || (list.length < 1))
        {
            return false;
        }

        String message_id_that = list[0];

        return message_id_this.equals(message_id_that);
    }

    public boolean equals(Message message)
    {
        String message_id_this = this.getMessageID();
        String message_id_that = message.getMessageID();

        return message_id_this.equals(message_id_that);
    }

    /**
     * <strong>Warning this is a <em>CLEANED</em> group of Headers</strong>
     * That is those Headers whose value is <em>not</em> null!
     */
    public Iterable<Header> getAllIterableHeaders()
    {
        List<Header> l = new ArrayList<Header>();

        try
        {
            Enumeration enumm = message.getAllHeaders();

            for (; enumm.hasMoreElements();)
            {
                Header h = (Header) enumm.nextElement();

                if (h.getValue() != null)
                {
                    l.add(h);
                }
            }
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }

        return l;
    }

    public Map<String,String> getAllSimpleHeaders()
    {
        TreeMap<String,String> simple_headers = new TreeMap<String,String>();

        try
        {
            Enumeration enumm = message.getAllHeaders();

            for (; enumm.hasMoreElements();)
            {
                Header h = (Header) enumm.nextElement();
                simple_headers.put(h.getName(), h.getValue());
            }
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }

        return simple_headers;
    }

    public InternetHeaders getAllInternetHeaders()
    {
        InternetHeaders internet_headers = new InternetHeaders();

        try
        {
            Enumeration enumm = message.getAllHeaders();

            for (; enumm.hasMoreElements();)
            {
                Header h = (Header) enumm.nextElement();
                internet_headers.addHeader(h.getName(), h.getValue());
            }
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }

        return internet_headers;
    }

    public String getHeader(String header)
    {
        String[] list = null;

        try
        {
            list = message.getHeader(header);
        }
        catch (MessagingException e)
        {
        }

        if ((list == null) || (list.length < 1))
        {
            return null;
        }

        return list[0];
    }

    private ByteBuf decendMultipart(MimeMultipart mm) throws MessagingException
    {
        ByteBuf buf = new ByteBuf();
        int max = mm.getCount();

        for (int i = 0; i < max; i++)
        {
            BodyPart bp = mm.getBodyPart(i);

            try
            {
                Object o = bp.getContent();

                if (o instanceof String)
                {
                    buf.append(o);
                }
                else
                {
                    buf.append("<a href=\"attachment:?" + i + "\">Attachment " +
                        i + "</a>\n");
                }
            }
            catch (IOException ee)
            {
                throw new MessagingException("I/O error", ee);
            }
        }

        return buf;
    }

    public boolean getFlag(Flag flag)
    {
        try
        {
            return message.isSet(flag);
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));

            return false;
        }
    }

    public boolean getFlags(Flags flags)
    {
        try
        {
            return message.getFlags().contains(flags);
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));

            return false;
        }
    }

    public InputStream getSource()
    {
        try
        {
            InternetHeaders heads = new InternetHeaders();
            Enumeration e;

            for (e = message.getAllHeaders(); e.hasMoreElements();)
            {
                Header h = (Header) e.nextElement();

                try
                {
                    heads.addHeader(h.getName(),
                        MimeUtility.encodeText(h.getValue()));
                }
                catch (UnsupportedEncodingException u)
                {
                    heads.addHeader(h.getName(), h.getValue()); // Anyone got a better
                                                                // idea???  ###
                }
            }

            ByteBuf buf = new ByteBuf();

            for (e = heads.getAllHeaderLines(); e.hasMoreElements();)
            {
                buf.append(e.nextElement());
                buf.append(Constants.BYTEBUFLINEBREAK);
            }

            buf.append(Constants.BYTEBUFLINEBREAK);

            ByteBuf buf2 = new ByteBuf();

            try
            {
                Object o = message.getContent();

                if (o instanceof MimeMultipart)
                {
                    MimeMultipart mm = (MimeMultipart) o;
                    ByteArrayOutputStream baos = new ByteArrayOutputStream();
                    mm.writeTo(baos);
                    buf2.append(baos.toByteArray());
                }
                else
                {
                    buf2.append(o);
                }
            }
            catch (IOException ee)
            {
                throw new MessagingException("I/O error", ee);
            }

            buf.append(buf2);

            return buf.makeInputStream();
        }
        catch (Exception e)
        {
            ByteBuf buf = new ByteBuf();
            buf.append(e);

            return buf.makeInputStream();
        }
    }

    public boolean isDeleted()
    {
        return getFlag(Flags.Flag.DELETED);
    }

    public Object[] messageThreadReferences()
    {
        // ### WRONG WRONG WRONG.  This needs to steal the code from
        // MessageBase.internReferences and do clever things with it.
        return null;
    }

    public void setFlag(Flag flag, boolean state)
    {
        try
        {
            message.setFlag(flag, state);
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }

    public void setFlags(Flags flags, boolean state)
    {
        try
        {
            message.setFlags(flags, state);
        }
        catch (MessagingException ex)
        {
            NoticeBoard.publish(new ExceptionNotice(ex));
        }
    }

    public void delete()
    {
        setFlag(Flags.Flag.DELETED, true);
        dispatcher.dispatch(new MessageDeletedEvent(this));
    }

    public Server getServer()
    {
        if (server == null)
        {
            server = folder.getServer();
        }

        return server;
    }

    public NewMessage replyTo(boolean replyToAll)
    {
        try
        {
            return new NewMessage(message.reply(replyToAll), message, this);
        }
        catch (Exception e)
        {
            NoticeBoard.publish(new ExceptionNotice(e));

            return null;
        }
    }

    public InputStream renderToHTML() throws IOException
    {
        return Renderer.render(message);
    }

    public String toString()
    {
        return getMessageID() + " (" + super.toString() + ")";
    }

    public void addMessageEventListener(MessageListener ml)
    {
        dispatcher.listeners.add(ml);
    }

    public void removeMessageEventListener(MessageListener ml)
    {
        dispatcher.listeners.remove(ml);
    }

    class EventDispatcher implements Runnable
    {
        Vector<MessageListener> listeners = new Vector<MessageListener>();
        private Event e;

        EventDispatcher()
        {
        }

        public void dispatch(MessageEvent e)
        {
            this.e = e;
            new Thread(this).start();
        }

        public void run()
        {
            for (MessageListener ml : listeners)
            {
                try
                {
                    if (e instanceof FlagChangedEvent)
                    {
                        ml.flagChanged((FlagChangedEvent) e);
                    }
                    else if (e instanceof MessageDeletedEvent)
                    {
                        ml.messageDeleted((MessageDeletedEvent) e);
                    }
                    else if (e instanceof MessageMovedEvent)
                    {
                        ml.messageMoved((MessageMovedEvent) e);
                    }
                }
                catch (Exception e)
                {
                    NoticeBoard.publish(new ExceptionNotice(e));
                }
            }
        }
    }
}
