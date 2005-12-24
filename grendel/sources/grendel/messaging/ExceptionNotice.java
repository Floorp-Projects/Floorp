/*
 * ExceptionNotice.java
 * 
 * Created on 19 August 2005, 15:19
 * 
 * This file contains code from GNU Classpath.
 * Copyright (C) 1998, 1999, 2002, 2004, 2005  Free Software Foundation, Inc.
 * 
 * GNU Classpath is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Classpath is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Classpath; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 * 
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library.  Thus, the terms and
 * conditions of the GNU General Public License cover the whole
 * combination.
 * 
 * As a special exception, the copyright holders of this library give you
 * permission to link this library with independent modules to produce an
 * executable, regardless of the license terms of these independent
 * modules, and to copy and distribute the resulting executable under
 * terms of your choice, provided that you also meet, for each linked
 * independent module, the terms and conditions of the license of that
 * module.  An independent module is a module which is not derived from
 * or based on this library.  If you modify this library, you may extend
 * this exception to your version of the library, but you are not
 * obligated to do so.  If you do not wish to do so, delete this
 * exception statement from your version.
 */

package grendel.messaging;


/**
 * @author hash9
 */
public class ExceptionNotice extends Notice {
    Throwable t;
    
    /** Creates a new instance of ExceptionNotice */
    public ExceptionNotice(Throwable t) {
        this.t = t;
    }
    
    /**
     * Create whole stack trace in a stringbuffer so we don't have to print
     * it line by line. This prevents printing multiple stack traces from
     * different threads to get mixed up when written to the same PrintWriter.
     */
    private String stackTraceString() {
        StringBuffer sb=new StringBuffer();
        
        // Main stacktrace
        StackTraceElement[] stack=t.getStackTrace();
        stackTraceStringBuffer(sb, t.toString(), stack, 0);
        
        // The cause(s)
        Throwable cause=t.getCause();
        
        while (cause!=null) {
            // Cause start first line
            sb.append("Caused by: ");
            
            // Cause stacktrace
            StackTraceElement[] parentStack=stack;
            stack=cause.getStackTrace();
            
            if ((parentStack==null)||(parentStack.length==0)) {
                stackTraceStringBuffer(sb, cause.toString(), stack, 0);
            } else {
                int equal=0; // Count how many of the last stack frames are equal
                int frame=stack.length-1;
                int parentFrame=parentStack.length-1;
                
                while ((frame>0)&&(parentFrame>0)) {
                    if (stack[frame].equals(parentStack[parentFrame])) {
                        equal++;
                        frame--;
                        parentFrame--;
                    } else {
                        break;
                    }
                }
                
                stackTraceStringBuffer(sb, cause.toString(), stack, equal);
            }
            
            cause=cause.getCause();
        }
        
        return sb.toString();
    }
    
    /**
     * Adds to the given StringBuffer a line containing the name and
     * all stacktrace elements minus the last equal ones.
     */
    private static void stackTraceStringBuffer(StringBuffer sb, String name,
            StackTraceElement[] stack, int equal) {
        String nl = "\n";
        // (finish) first line
        sb.append(name);
        sb.append(nl);
        
        // The stacktrace
        if (stack == null || stack.length == 0) {
            sb.append("   <<No stacktrace available>>");
            sb.append(nl);
        } else {
            for (int i = 0; i < stack.length-equal; i++) {
                sb.append("   at ");
                sb.append(stack[i] == null ? "<<Unknown>>" : stack[i].toString());
                sb.append(nl);
            }
            if (equal > 0) {
                sb.append("   ...");
                sb.append(equal);
                sb.append(" more");
                sb.append(nl);
            }
        }
    }
    
    public String toString() {
        return stackTraceString();
    }
}
