/*
 * SplitInputStream.java
 *
 * Created on 3. september 2000, 13:25
 */

package org.mozilla.translator.io;

import java.io.*;
/**
 *
 * @author  Henrik Lynggaard
 * @version 
 */
public class SplitInputStream extends InputStream {

    InputStream currentStream;
    /** Creates new SplitInputSt ream */
    public SplitInputStream(InputStream is) 
    {
        super();
        currentStream = is;
    }

    public int available() throws IOException
    {
        return currentStream.available();
    }

        
    public void close() throws IOException
    {
        // does nothing, and shoudl do nothing;
    }
    
    public void mark(int readlimit)
    {
        currentStream.mark(readlimit);
    }
    
    public boolean markSupported()
    {
        return currentStream.markSupported();
    }
    
    
    public int read() throws IOException
    {
        return currentStream.read();
    }
    
    public int read(byte[] b) throws IOException
    {
        return currentStream.read(b);
    }
    
    public int read(byte[] b, int off, int len) throws IOException
    {
        return currentStream.read(b,off,len);
    }
    
    public void reset() throws IOException
    {
        currentStream.reset();
    }
    
    public long skip(long n) throws IOException
    {
        return currentStream.skip(n);
    }
    
    


    
}
