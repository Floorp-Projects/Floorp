/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 */

package org.mozilla.webclient;

/*
 * RandomHTMLInputStream.java
 */

import org.mozilla.util.Assert;
import org.mozilla.util.ParameterCheck;

import java.io.InputStream;
import java.io.IOException;
import java.util.Random;

/**

 * This class simulates a nasty, misbehavin' InputStream.

 * It randomly throws IOExceptions, blocks on read, and is bursty.

 */ 

public class RandomHTMLInputStream extends InputStream
{

//
// Class variables
//

private static final int MAX_AVAILABLE = 4096;

/**

 * This makes it so only when we get a random between 0 and 100 number
 * that evenly divides by three do we throw an IOException

 */

private static final int EXCEPTION_DIVISOR = 179;

private static final byte [] CHARSET;

//
// relationship ivars
//

private Random random;

//
// attribute ivars
//

private boolean isClosed;

private boolean firstRead;

private boolean randomExceptions = true;

/**

 * the number of times that read(bytearray) can be called and still get
 * data.  

 */

private int numReads;

private int available;

/**    

 * @param yourNumReads must be at least 2

 */

static {
    String charSet = " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890[]{}";
    CHARSET = charSet.getBytes();
}

public RandomHTMLInputStream(int yourNumReads, boolean yourRandomExceptions)
{
    ParameterCheck.greaterThan(yourNumReads, 1);
    randomExceptions = yourRandomExceptions;

    random = new Random(1234);
    Assert.assert_it(null != random);

    isClosed = false;
    firstRead = true;
    numReads = yourNumReads;
    available = random.nextInt(MAX_AVAILABLE);
}

public int available() throws IOException
{
    int result;
    if (shouldThrowException()) {
        throw new IOException("It's time for an IOException!");
    }
    if (isClosed) {
        result = -1;
    }
    else {
        result = available;
    }
    return result;
}

public int read() throws IOException
{
    int result = 0;
    if (shouldThrowException()) {
        throw new IOException("It's time for an IOException!");
    }

    if (0 < available) {
        result = (int) 'a';
        available--;
    }
    else {
        result = -1;
    }
    return result;
}

public int read(byte[] b, int off, int len) throws IOException
{
    if (shouldThrowException()) {
        throw new IOException("It's time for an IOException!");
    }

    byte [] bytes;
    int i = 0;
    int max = 0;
    int numRead = -1;

    // write case 0, no more reads left
    if (0 == numReads || isClosed) {
        return -1;
    }
    if (len <= available) {
        max = len;
    }
    else {
        max = available;
    }
    

    if (firstRead) {
        String htmlHead = "<HTML><BODY><PRE>START Random Data";
        numRead = htmlHead.length();

        // write case 1, yes enough length to write htmlHead
        if (numRead < len && len <= available) {
            bytes = htmlHead.getBytes();
            for (i = 0; i < numRead; i++) {
                b[off+i] = bytes[i];
            }
            available -= numRead;
        }
        else {
            // write case 2, not enough length to write htmlHead
            for (i = 0; i < max; i++) {
                b[off+i] = (byte) random.nextInt(8);
            }
            numRead = max;
            available -= max;
        }
        firstRead = false;
    }
    else {
        // if this is the last read
        if (1 == numReads) {
            String htmlTail = "\nEND Random Data</PRE></BODY></HTML>";
            numRead = htmlTail.length();
            // write case 3, yes enough length to write htmlTail
            if (numRead < len && len <= available) {
                bytes = htmlTail.getBytes();
                for (i = 0; i < numRead; i++) {
                    b[off+i] = bytes[i];
                }
                available -= numRead;
            }
            else {
                // write case 4, not enough length to write htmlTail
                for (i = 0; i < max; i++) {
                    b[off+i] = (byte) random.nextInt(8);
                }
                numRead = max;
                available -= max;
            }
            try {
                synchronized(this) {
                    this.notify();
                }
            }
            catch (Exception e) {
                throw new IOException("RandomHTMLInputStream: Can't notify listeners");
            }
        }
        else {
            
            // if it's time to block
            if (random.nextBoolean()) {
                try {
                    System.out.println("RandomHTMLInputStream:: sleeping");
                    System.out.flush();
                    Thread.sleep(3000);
                }
                catch (Exception e) {
                    throw new IOException(e.getMessage());
                }
            }

            // write case 5, write some random bytes to fit length
            
            // this is not the first or the last read, just cough up
            // some random bytes.

            bytes = new byte[max];
            for (i = 0; i < max; i++) {
                if (0 == (i % 78)) {
                    b[off+i] = (byte) '\n';
                }
                else {
                    b[off+i] = CHARSET[random.nextInt(CHARSET.length)];
                }
            }
            numRead = max;
            available -= max;
        }
    }
    available = random.nextInt(MAX_AVAILABLE);
    numReads--;
    return numRead;
}

public void close() throws IOException
{
    if (shouldThrowException()) {
        throw new IOException("It's time for an IOException!");
    }
    isClosed = true;
    try {
        synchronized(this) {
            this.notify();
        }
    }
    catch (Exception e) {
        throw new IOException("RandomHTMLInputStream: Can't notify listeners");
    }
}

private boolean shouldThrowException()
{
    if (!randomExceptions) {
        return false;
    }
    int nextInt = random.nextInt(10000);

    boolean result = false;

    if (nextInt > EXCEPTION_DIVISOR) {
        result = (0 == (nextInt % EXCEPTION_DIVISOR));
    }
    return result;
}

}
