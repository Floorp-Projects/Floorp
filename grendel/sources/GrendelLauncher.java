import java.io.File;
import java.io.FileFilter;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import javax.swing.filechooser.FileSystemView;
/*
 * GrendelLoader.java
 *
 * Created on 22 December 2005, 20:04
 *
 */

/**
 * This Class contains all the Inital Startup Logic for Grendel.
 * <strong>DO NOT restucture Grendel's build without checking this class.</strong>
 * @author hash9
 */
public final class GrendelLauncher {
    
    private static final String extlib = "bin/lib/";
    
    private static final String glib = "bin/";
    
    private static final String resources = "resources/";
    
    /** Creates a new instance of GrendelLoader */
    private GrendelLauncher() {
    }
    
    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws /*MalformedURLException*/ Exception, Throwable {
        Thread ct = Thread.currentThread();
        
        ClassLoader ccl = ct.getContextClassLoader();
        
        URLClassLoader ext_urlcl = new URLClassLoader(fromJars(fromDirs(extlib)),ccl);
        URLClassLoader g_urlcl = new URLClassLoader(fromJars(fromDirs(glib)),ext_urlcl);
        
        /*URLClassLoader ext_urlcl = new URLClassLoader(concat(fromJars(fromDirs(glib)),fromJars(fromDirs(extlib))),ccl);*/
        
        ct.setContextClassLoader(g_urlcl);
        //System.setProperty("user.dir",new File(resources).getAbsolutePath());
        try {
            Class g_M = g_urlcl.loadClass("grendel.Main");
            Method g_M_m = g_M.getMethod("main",String[].class);
            g_M_m.invoke(null, new Object[] {args});
            } catch (InvocationTargetException ex) {
                throw ex.getTargetException();
            }
        /*} catch (InvocationTargetException ex) {
            System.err.println("== Grendel Brought An Unhandeled Exception ==");
            System.err.println("== This is a bug, please report it. ==");
            ex.getTargetException().printStackTrace();
        } catch (Exception ex) {
            ex.printStackTrace();
        }*/
    }
    
    private static File[] fromDirs(String dir_s) {
        File dir = new File(dir_s);
        
        File[] f_a = dir.listFiles();
        ArrayList<File> f_l = new ArrayList<File>();
	for (int i = 0 ; i < f_a.length ; i++) {
	    File f = f_a[i];
	    if (f.isFile() && f.getName().endsWith(".jar")) {
		f_l.add(f);
	    }
	}
	return (File[])(f_l.toArray(new File[0]));
    }
    
    private static URL[] fromJars(File[] files) throws MalformedURLException {
        if (files==null) {
            return new URL[0];
        }
        URL[] url_a =  new URL[files.length];
        for (int i = 0; i < files.length; i++) {
            File f = files[i];
            url_a[i] = f.toURI().toURL();
        }
        return url_a;
    }
    
    private static URL[] concat(URL[] a ,URL[] b) {
        URL[] s =  new URL[a.length+b.length];
        System.arraycopy(a,0,s,0,a.length);
        System.arraycopy(b,0,s,a.length,b.length);
        return s;
    }
    
}
