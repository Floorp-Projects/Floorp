/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbException.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbException extends Exception
{
    // methods
    //

    public DbException(String s)
    {
        super(s);
    }

    public DbException(String s, int errno)
    {
        super(s);
        this.errno_ = errno;
    }

    // get/set methods
    //

    public int get_errno()
    {
        return errno_;
    }

    // private data
    //

    private int errno_;
}

// end of DbException.java
