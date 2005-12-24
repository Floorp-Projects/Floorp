/*
 * JMProvider.java
 *
 * Created on 04 September 2005, 18:07
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package grendel.javamail;
import grendel.prefs.Preferences;
import grendel.prefs.accounts.Account;
import grendel.prefs.accounts.Account__Local;
import grendel.prefs.accounts.Account__Send;
import grendel.prefs.accounts.Account__Server;
import grendel.prefs.xml.XMLPreferences;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;
import java.util.TreeSet;
import javax.mail.NoSuchProviderException;
import javax.mail.Provider;
import javax.mail.Service;
import javax.mail.Session;
import javax.mail.Store;
import javax.mail.Transport;
import javax.mail.URLName;



/**
 *
 * @author hash9
 */
public final class JMProviders {
    private static JMProviders jmProvider;
    private static Session sess;
    private static XMLPreferences options_mail;
    
    private Map<String, List<String>> order = new TreeMap<String, List<String>>();
    
    /**
     * Map<Vendor_var(String), Map<Protocal(String), Provider(Provider)>>
     *
     */
    private Map<String, Map<String, Provider>> sorted_providers = new TreeMap<String, Map<String, Provider>>();
    
    public static JMProviders getJMProviders() {
        if (jmProvider==null) {
            jmProvider = new JMProviders();
        }
        return jmProvider;
    }
    
    public static Session getSession() {
        if (sess==null) {
            options_mail=Preferences.getPreferances().getPropertyPrefs("options").getPropertyPrefs("mail");
            Properties props = new Properties();
            sess=Session.getDefaultInstance(props, null);
            //fSession.setDebug(options_mail.getPropertyBoolean("debug"));
            sess.setDebug(true);
        }
        return sess;
    }
    
    private JMProviders() {
        loadSelection();
    }
    
    private void loadSelection() {
        try {
            InputStream is = JMProviders.class.getResourceAsStream("/META-INF/javamail.selection");
            if (is==null) {
                System.err.println("ERROR: /META-INF/javamail.selection does not exist.");
                System.err.println("ERROR: Cannot continue: Exitting");
                System.exit(4);
            }
            BufferedReader br = new BufferedReader(new InputStreamReader(is));
            while (br.ready()) {
                String line = br.readLine().trim();
                if (line.equals("")) {
                    // Blank: Ignore the line
                } else if (line.startsWith("#")) {
                    // Comment: Ignore the line
                } else if (line.contains(";")) { // Selection Line
                    int split = line.indexOf(';');
                    String protocal = line.substring(0,split).trim().toLowerCase();
                    String order_sl = line.substring(split+1).trim().toLowerCase();
                    List<String> order_l = Arrays.asList(order_sl.split(","));
                    for (int i = 0; i < order_l.size();i++) {
                        if (order_l.get(i).equals("")) {
                            order_l.remove(i);
                        }
                    }
                    order.put(protocal,order_l);
                } else { // Other line: issue warning
                    System.err.println("Line: \""+line+"\" is Invalid!");
                }
            }
        } catch (IOException ioe) {
            System.err.println("ERROR: " + ioe.getLocalizedMessage());
            System.err.println("ERROR: Whilest processing: /META-INF/javamail.selection");
            System.err.println("ERROR: Cannot continue: Exitting");
            System.exit(4);
        }
    }
      
    private Service getService(URLName url, Account a) throws NoSuchProviderException {
        String protocal = url.getProtocol();
        List<String> order_l = order.get(protocal);
        if ((order_l==null)||(order_l.size()==0)) {
            throw new NoSuchProviderException("No Provider for " + protocal);
        }
        
        for (String vendor_var: order_l) {
            try {
                return getService(url,a,vendor_var);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        
        throw new NoSuchProviderException("No Provider for " + protocal);
    }
    
    private Service getService(URLName url, Account a, String vendor) throws NoSuchProviderException {
        vendor = vendor.toLowerCase();
        
        if (vendor.equals("javamail")) { return getService(url); }
        
        vendor = Character.toUpperCase(vendor.charAt(0))+vendor.substring(1);
        Exception cause = null;
        try {
            Class c = Thread.currentThread().getContextClassLoader().loadClass(this.getClass().getPackage().getName()+"."+vendor);
            Method m = c.getMethod("get_"+url.getProtocol().toLowerCase(),URLName.class, Account.class);
            if (Service.class.isAssignableFrom(m.getReturnType())) {
                Service s = (Service) m.invoke(null,url,a);
                return s;
            }
        } catch (Exception e) {
            // It broke, it must not exist
            cause=e;
        }
        
        NoSuchProviderException nspe = new NoSuchProviderException("Failed to get Session. See Cause.");
        if (cause!=null) {
            nspe.initCause(cause);
        }
        throw nspe;
    }
    
    private Service getService(URLName url) throws NoSuchProviderException {
        try {
            return getSession().getStore(url);
        } catch (NoSuchProviderException nspe) {
            return getSession().getTransport(url);
        }
    }
    
    /**
     * Get a Transport object that implements the specified protocol.
     * If an appropriate Transport object cannot be obtained, null is
     * returned.
     *
     *
     * @return a Transport object
     * @exception NoSuchProviderException If provider for the given
     * 			protocol is not found.
     */
    public Transport getTransport(Account__Send account) throws NoSuchProviderException {
        String host = null;
        int port = -1;
        String folder = null;
        if (account instanceof Account__Server) {
            host = ((Account__Server)account).getHost();
            port = ((Account__Server)account).getPort();
            if (port == 0) {
                port = -1;
            }
        }
        if (account instanceof Account__Local) {
            folder=((Account__Local)account).getStoreDirectory();
        }
        URLName url = new URLName(account.getTransport(),host,port,folder,null,null);
        Service s = getService(url,account);
        if (s instanceof Transport) {
            return (Transport) s;
        }
        throw new NoSuchProviderException("Invalid Service Type: "+s.toString());
    }
    
    /**
     * Get a Store object that implements the specified protocol. If an
     * appropriate Store object cannot be obtained,
     * NoSuchProviderException is thrown.
     *
     * @param	        account
     * @return		a Store object
     * @exception	NoSuchProviderException If a provider for the given
     *			protocol is not found.
     */
    public Store getStore(Account account) throws NoSuchProviderException {
        String host = null;
        int port = -1;
        String folder = null;
        if (account instanceof Account__Server) {
            host = ((Account__Server)account).getHost();
            port = ((Account__Server)account).getPort();
            if (port == 0) {
                port = -1;
            }
        }
        if (account instanceof Account__Local) {
            folder=((Account__Local)account).getStoreDirectory();
        }
        URLName url = new URLName(account.getType(),host,port,folder,null,null);
        Service s = getService(url,account);
        if (s instanceof Store) {
            return (Store) s;
        }
        throw new NoSuchProviderException("Invalid Service Type: "+s.toString());
    }
}
