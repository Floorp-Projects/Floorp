/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)AccessExample.cpp	10.6 (Sleepycat) 4/10/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <iostream.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#endif

#include <db_cxx.h>

class AccessExample : public DbEnv
{
public:
    void run();

    AccessExample(const char *home);

private:
    static const char FileName[];

    // no need for copy and assignment
    AccessExample(const AccessExample &);
    operator = (const AccessExample &);
};

static void usage();          // forward

int main(int argc, char *argv[])
{
    const char *home = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0)
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
        AccessExample app(home);
        app.run();
        return 0;
    }
    catch (DbException &dbe)
    {
        cerr << "AccessExample: " << dbe.what() << "\n";
        return 1;
    }
}

static void usage()
{
    cerr << "usage: AccessExample [-h home]\n";
    exit(1);
}

const char AccessExample::FileName[] = "access.db";

AccessExample::AccessExample(const char *home)
:   DbEnv(home, 0, 0)
{
    set_error_stream(&cerr);
    set_errpfx("AccessExample");
}

void AccessExample::run()
{
    Db *table;
    Db::open(FileName, DB_BTREE, DB_CREATE, 0644, this, 0, &table);

    //
    // Insert records into the database, where the key is the user
    // input and the data is the user input in reverse order.
    //
    char buf[1024];
    char rbuf[1024];

    for (;;) {
        cout << "input> ";
        cout.flush();

        cin.getline(buf, sizeof(buf));
        if (cin.eof())
            break;

        int len = strlen(buf);
        if (len <= 0)
            continue;

        char *t = rbuf;
        char *p = buf + len - 1;
        while (p >= buf)
            *t++ = *p--;
        *t = '\0';

        Dbt key(buf, len+1);
        Dbt data(rbuf, len+1);

        try
        {
            int err;
            if ((err = table->put(0, &key, &data, 0)) == DB_KEYEXIST) {
                cout << "Key " << buf << " already exists.\n";
            }
        }
        catch (DbException &dbe)
        {
            cout << dbe.what() << "\n";
        }
        cout << "\n";
    }

    // Acquire an iterator for the table.
    Dbc *iterator;
    table->cursor(NULL, &iterator);

    // Walk through the table, printing the key/data pairs.
    Dbt key;
    Dbt data;
    while (iterator->get(&key, &data, DB_NEXT) == 0)
    {
        char *key_string = (char *)key.get_data();
        char *data_string = (char *)data.get_data();
        cout << key_string << " : " << data_string << "\n";
    }
    iterator->close();
    table->close(0);
}
