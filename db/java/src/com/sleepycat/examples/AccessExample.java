/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)AccessExample.java	10.5 (Sleepycat) 4/10/98
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.IOException;

class AccessExample extends DbEnv
{
    private static final String FileName = "access.db";

    public AccessExample(String home)
         throws DbException
    {
        super(home, null, 0);

        try {
            set_error_stream(System.err);
            set_errpfx("AccessExample");
        }
        catch (Exception e) {
            System.err.println(e.toString());
            System.exit(1);
        }
    }

    private static void usage()
    {
        System.err.println("usage: AccessExample [-h home]\n");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        String home = null;
        for (int i = 0; i < argv.length; ++i)
        {
            if (argv[i].equals("-h"))
            {
                home = argv[++i];
            }
            else
            {
                usage();
            }
        }

        try
        {
            AccessExample app = new AccessExample(home);
            app.run();
        }
        catch (DbException dbe)
        {
            System.err.println("AccessExample: " + dbe.toString());
            System.exit(1);
        }
        System.exit(0);
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt)
    {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    static public String getLine(InputStreamReader reader)
    {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        }
        catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    public void run()
         throws DbException
    {
        Db table = Db.open(FileName, Db.DB_BTREE, Db.DB_CREATE, 0644, this, null);

        //
        // Insert records into the database, where the key is the user
        // input and the data is the user input in reverse order.
        //
        InputStreamReader reader = new InputStreamReader(System.in);

        for (;;) {
            String line = askForLine(reader, System.out, "input> ");
            if (line == null)
                break;

            byte buf[] = line.getBytes();
            byte rbuf[] = (new StringBuffer(line)).reverse().
                                toString().getBytes();

            Dbt key = new Dbt(buf);
            key.set_size(buf.length);
            Dbt data = new Dbt(rbuf);
            data.set_size(rbuf.length);

            try
            {
                int err;
                if ((err = table.put(null, key, data, 0)) == Db.DB_KEYEXIST) {
                    System.out.println("Key " + buf + " already exists.");
                }
            }
            catch (DbException dbe)
            {
                System.out.println(dbe.toString());
            }
            System.out.println("");
        }

        // Acquire an iterator for the table.
        Dbc iterator;
        iterator = table.cursor(null);

        // Walk through the table, printing the key/data pairs.
        // We use the DB_DBT_MALLOC flag to ask DB to allocate
        // byte arrays for the results.
        Dbt key = new Dbt();
        key.set_flags(Db.DB_DBT_MALLOC);
        Dbt data = new Dbt();
        data.set_flags(Db.DB_DBT_MALLOC);
        while (iterator.get(key, data, Db.DB_NEXT) == 0)
        {
            String key_string = new String(key.get_data(), 0, key.get_size());
            String data_string =
                new String(data.get_data(), 0, data.get_size());
            System.out.println(key_string + " : " + data_string);
        }
        iterator.close();
        table.close(0);
    }
}
