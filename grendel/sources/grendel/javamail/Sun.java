/*
 * Sun.java
 *
 * Created on 04 September 2005, 21:31
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.javamail;
import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account_IMAP;
import grendel.prefs.accounts.Account_POP3;
import grendel.prefs.accounts.Account_SMTP;
import java.util.Properties;
import javax.mail.Service;
import javax.mail.Session;
import javax.mail.URLName;

/**
 *
 * @author hash9
 */
public final class Sun {
    public static Service get_smtp(URLName url, Account a) {
        Account_SMTP smtp = (Account_SMTP) a;
        Properties props = JMProviders.getSession().getProperties();
        // Name                             Type      Description
        // mail.smtp.user                   String    Default user name for SMTP.
        // mail.smtp.host                   String    The SMTP server to connect to.
        // mail.smtp.port                   int       The SMTP server port to connect to, if the connect() method doesn't explicitly specify one. Defaults to 25.
        // mail.smtp.connectiontimeout      int       Socket connection timeout value in milliseconds. Default is infinite timeout.
        props.put("mail.smtp.connectiontimeout","10000"); //XXX This should be an option
        // mail.smtp.timeout                int       Socket I/O timeout value in milliseconds. Default is infinite timeout.
        props.put("mail.smtp.timeout","10000"); //XXX This should be an option
        // mail.smtp.from                   String    Email address to use for SMTP MAIL command. This sets the envelope return address. Defaults to msg.getFrom() or InternetAddress.getLocalAddress(). NOTE: mail.smtp.user was previously used for this.
        // mail.smtp.localhost              String    Local host name used in the SMTP HELO or EHLO command. Defaults to InetAddress.getLocalHost().getHostName(). Should not normally need to be set if your JDK and your name service are configured properly.
        // mail.smtp.localaddress           String    Local address (host name) to bind to when creating the SMTP socket. Defaults to the address picked by the Socket class. Should not normally need to be set, but useful with multi-homed hosts where it's important to pick a particular local address to bind to.
        // mail.smtp.localport              int       Local port number to bind to when creating the SMTP socket. Defaults to the port number picked by the Socket class.
        // mail.smtp.ehlo                   boolean   If false, do not attempt to sign on with the EHLO command. Defaults to true. Normally failure of the EHLO command will fallback to the HELO command; this property exists only for servers that don't fail EHLO properly or don't implement EHLO properly.
        props.put("mail.smtp.ehlo","true");
        // mail.smtp.auth                   boolean   If true, attempt to authenticate the user using the AUTH command. Defaults to false.
        if ((smtp.getUsername()!=null)&&(smtp.getPassword()!=null)) {
            props.put("mail.smtp.auth","required");
        }
        // mail.smtp.submitter              String    The submitter to use in the AUTH tag in the MAIL FROM command. Typically used by a mail relay to pass along information about the original submitter of the message. See also the setSubmitter method of SMTPMessage. Mail clients typically do not use this.
        // mail.smtp.dsn.notify             String    The NOTIFY option to the RCPT command. Either NEVER, or some combination of SUCCESS, FAILURE, and DELAY (separated by commas).
        // mail.smtp.dsn.ret                String    The RET option to the MAIL command. Either FULL or HDRS.
        // mail.smtp.allow8bitmime          boolean   if set to true, and the server supports the 8BITMIME extension, text parts of messages that use the "quoted-printable" or "base64" encodings are converted to use "8bit" encoding if they follow the RFC2045 rules for 8bit text.
        // mail.smtp.sendpartial            boolean   If set to true, and a message has some valid and some invalid addresses, send the message anyway, reporting the partial failure with a SendFailedException. If set to false (the default), the message is not sent to any of the recipients if there is an invalid recipient address.
        props.put("mail.smtp.sendpartial","true"); //XXX This should be desided as a bug! ###
        // mail.smtp.sasl.realm             String    The realm to use with DIGEST-MD5 authentication.
        // mail.smtp.quitwait               boolean   if set to true, causes the transport to wait for the response to the QUIT command. If set to false (the default), the QUIT command is sent and the connection is immediately closed. (NOTE: The default may change in the next release.)
        props.put("mail.smtp.quitwait","true"); //XXX This should be desided as a bug! ###
        // mail.smtp.reportsuccess          boolean   If set to true, causes the transport to include an SMTPAddressSucceededException for each address that is successful. Note also that this will cause a SendFailedException to be thrown from the sendMessage method of SMTPTransport even if all addresses were correct and the message was sent successfully.
        // mail.smtp.socketFactory.class    String    If set, specifies the name of a class that implements the javax.net.SocketFactory interface. This class will be used to create SMTP sockets.
        // mail.smtp.socketFactory.fallback boolean   If set to true, failure to create a socket using the specified socket factory class will cause the socket to be created using the java.net.Socket class. Defaults to true.
        // mail.smtp.socketFactory.port     int       Specifies the port to connect to when using the specified socket factory. If not set, the default port will be used.
        // mail.smtp.mailextension          String    Extension string to append to the MAIL command. The extension string can be used to specify standard SMTP service extensions as well as vendor-specific extensions. Typically the application should use the SMTPTransport method supportsExtension to verify that the server supports the desired service extension. See RFC 1869 and other RFCs that define specific extensions.
        Session s = JMProviders.getSession().getInstance(props);
        return new com.sun.mail.smtp.SMTPTransport(s,url);
    }
    
    public static Service get_imap(URLName url, Account a) {
        Account_IMAP imap = (Account_IMAP)a;
        Properties props = JMProviders.getSession().getProperties();
        // Name                               Type       Description
        // mail.imap.user                     String     Default user name for IMAP.
        // mail.imap.host                     String     The IMAP server to connect to.
        // mail.imap.port                     int        The IMAP server port to connect to, if the connect() method doesn't explicitly specify one. Defaults to 143.
        // mail.imap.partialfetch             boolean    Controls whether the IMAP partial-fetch capability should be used. Defaults to true.
        // mail.imap.fetchsize                int        Partial fetch size in bytes. Defaults to 16K.
        // mail.imap.connectiontimeout        int        Socket connection timeout value in milliseconds. Default is infinite timeout.
        // mail.imap.timeout                  int        Socket I/O timeout value in milliseconds. Default is infinite timeout.
        // mail.imap.statuscachetimeout       int        Timeout value in milliseconds for cache of STATUS command response. Default is 1000 (1 second). Zero disables cache.
        // mail.imap.appendbuffersize         int        Maximum size of a message to buffer in memory when appending to an IMAP folder. If not set, or set to -1, there is no maximum and all messages are buffered. If set to 0, no messages are buffered. If set to (e.g.) 8192, messages of 8K bytes or less are buffered, larger messages are not buffered. Buffering saves cpu time at the expense of short term memory usage. If you commonly append very large messages to IMAP mailboxes you might want to set this to a moderate value (1M or less).
        // mail.imap.connectionpoolsize       int        Maximum number of available connections in the connection pool. Default is 1.
        // mail.imap.connectionpooltimeout    int        Timeout value in milliseconds for connection pool connections. Default is 45000 (45 seconds).
        // mail.imap.separatestoreconnection  boolean    Flag to indicate whether to use a dedicated store connection for store commands. Default is false.
        // mail.imap.allowreadonlyselect      boolean    If false, attempts to open a folder read/write will fail if the SELECT command succeeds but indicates that the folder is READ-ONLY. This sometimes indicates that the folder contents can'tbe changed, but the flags are per-user and can be changed, such as might be the case for public shared folders. If true, such open attempts will succeed, allowing the flags to be changed. The getMode method on the Folder object will return Folder.READ_ONLY in this case even though the open method specified Folder.READ_WRITE. Default is false.
        // mail.imap.auth.login.disable       boolean    If true, prevents use of the non-standard AUTHENTICATE LOGIN command, instead using the plain LOGIN command. Default is false.
        // mail.imap.auth.plain.disable       boolean    If true, prevents use of the AUTHENTICATE PLAIN command. Default is false.
        // mail.imap.starttls.enable          boolean    If true, enables the use of the STARTTLS command (if supported by the server) to switch the connection to a TLS-protected connection before issuing any login commands. Note that an appropriate trust store must configured so that the client will trust the server's certificate. This feature only works on J2SE 1.4 and newer systems. Default is false.
        // mail.imap.localaddress             String     Local address (host name) to bind to when creating the IMAP socket. Defaults to the address picked by the Socket class. Should not normally need to be set, but useful with multi-homed hosts where it's important to pick a particular local address to bind to.
        // mail.imap.localport                int        Local port number to bind to when creating the IMAP socket. Defaults to the port number picked by the Socket class.
        // mail.imap.sasl.enable              boolean    If set to true, attempt to use the javax.security.sasl package to choose an authentication mechanism for login. Defaults to false.
        // mail.imap.sasl.mechanisms          String     A space or comma separated list of SASL mechanism names to try to use.
        // mail.imap.sasl.authorizationid     String     The authorization ID to use in the SASL authentication. If not set, the authentication ID (user name) is used.
        // mail.imap.socketFactory.class      String     If set, specifies the name of a class that implements the javax.net.SocketFactory interface. This class will be used to create IMAP sockets.
        // mail.imap.socketFactory.fallback   boolean    If set to true, failure to create a socket using the specified socket factory class will cause the socket to be created using the java.net.Socket class. Defaults to true.
        // mail.imap.socketFactory.port       int        Specifies the port to connect to when using the specified socket factory. If not set, the default port will be used.
        Session s = JMProviders.getSession().getInstance(props);
        return new com.sun.mail.imap.IMAPStore(s,url);
    }
    
    public static Service get_pop3(URLName url, Account a) {
        Account_POP3 pop3 = (Account_POP3)a;
        Properties props = JMProviders.getSession().getProperties();
        Session s = JMProviders.getSession().getInstance(props);
        return new com.sun.mail.pop3.POP3Store(s,url);
    }
}
