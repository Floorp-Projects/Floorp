/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* use sequental numbers printed to strings
 * to store lots and lots of entries in the
 * database.
 *
 * Start with 100 entries, put them and then
 * read them out.  Then delete the first
 * half and verify that all of the first half
 * is gone and then verify that the second
 * half is still there.
 * Then add the first half back and verify
 * again.  Then delete the middle third
 * and verify again.
 * Then increase the size by 1000 and do
 * the whole add delete thing again.
 *
 * The data for each object is the number string translated
 * to hex and replicated a random number of times.  The
 * number of times that the data is replicated is the first
 * int32 in the data.
 */

#include <stdio.h>

#include <stdlib.h>
#ifdef STDC_HEADERS
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <string.h>
#include <assert.h>
#include "mcom_db.h"

DB *database=0;
int MsgPriority=5;

#if defined(_WINDOWS) && !defined(WIN32)
#define int32 long
#define uint32 unsigned long
#else
#define int32 int
#define uint32 unsigned int
#endif

typedef enum {
USE_LARGE_KEY,
USE_SMALL_KEY
} key_type_enum;

#define TraceMe(priority, msg) 		\
	do {							\
		if(priority <= MsgPriority)	\
		  {							\
			ReportStatus msg;		\
		  }							\
	} while(0)

int
ReportStatus(char *string, ...)
{
    va_list args;

#ifdef STDC_HEADERS
    va_start(args, string);
#else
    va_start(args);
#endif
    vfprintf(stderr, string, args);
    va_end(args);

	fprintf (stderr, "\n");

	return(0);
}

int
ReportError(char *string, ...)
{
    va_list args;

#ifdef STDC_HEADERS
    va_start(args, string);
#else
    va_start(args);
#endif
	fprintf (stderr, "\n	");
    vfprintf(stderr, string, args);
	fprintf (stderr, "\n");
    va_end(args);

	return(0);
}

DBT * MakeLargeKey(int32 num)
{
	int32 low_bits;
	static DBT rv;
	static char *string_rv=0;
	int rep_char;
	int32 size;

	if(string_rv)
		free(string_rv);

	/* generate a really large text key derived from
	 * an int32
	 */
	low_bits = (num % 10000) + 1;

	/* get the repeat char from the low 26 */
	rep_char = (char) ((low_bits % 26) + 'a');

	/* malloc a string low_bits wide */
	size = low_bits*sizeof(char);
	string_rv = (char *)malloc((size_t)size);

	memset(string_rv, rep_char, (size_t)size);

	rv.data = string_rv;
	rv.size = size;

	return(&rv);
}

DBT * MakeSmallKey(int32 num)
{
	static DBT rv;
	static char data_string[64];

	rv.data = data_string;

	sprintf(data_string, "%ld", (long)num);
	rv.size = strlen(data_string);

	return(&rv);

}

DBT * GenKey(int32 num, key_type_enum key_type)
{
	DBT *key;

	switch(key_type)
	  {
		case USE_LARGE_KEY:
			key = MakeLargeKey(num);
			break;
		case USE_SMALL_KEY:
			key = MakeSmallKey(num);
			break;
		default:
			abort();
			break;
	  }

	return(key);
}

int
SeqDatabase()
{
	int status;
	DBT key, data;

	ReportStatus("SEQuencing through database...");

	/* seq throught the whole database */
    if(!(status = (*database->seq)(database, &key, &data, R_FIRST)))
	  {
        while(!(status = (database->seq) (database, &key, &data, R_NEXT)));
			; /* null body */
	  }

	if(status < 0)
		ReportError("Error seq'ing database");

	return(status);
}

int 
VerifyData(DBT *data, int32 num, key_type_enum key_type)
{
	int32 count, compare_num;
	uint32 size;
	int32 *int32_array;

	/* The first int32 is count 
	 * The other n entries should
	 * all equal num
	 */
	if(data->size < sizeof(int32))
	  {
		ReportError("Data size corrupted");
		return -1;
	  }

	memcpy(&count, data->data, sizeof(int32));

	size = sizeof(int32)*(count+1);

	if(size != data->size)
	  {
		ReportError("Data size corrupted");
		return -1;
	  }

	int32_array = (int32*)data->data;

	for(;count > 0; count--)
	  {
		memcpy(&compare_num, &int32_array[count], sizeof(int32));

		if(compare_num != num)
	      {
		    ReportError("Data corrupted");
		    return -1;
	      }
	  }

	return(0);
}


/* verify that a range of number strings exist
 * or don't exist. And that the data is valid
 */
#define SHOULD_EXIST 1
#define SHOULD_NOT_EXIST 0
int
VerifyRange(int32 low, int32 high, int32 should_exist, key_type_enum key_type)
{
	DBT *key, data;
	int32 num;
	int status;

	TraceMe(1, ("Verifying: %ld to %ld, using %s keys", 
		    low, high, key_type == USE_SMALL_KEY ? "SMALL" : "LARGE"));

	for(num = low; num <= high; num++)
	  {

		key = GenKey(num, key_type);

		status = (*database->get)(database, key, &data, 0);

		if(status == 0)
		  {
			/* got the item */
			if(!should_exist)
			  {
				ReportError("Item exists but shouldn't: %ld", num);
			  }
			else
			  {
			    /* else verify the data */
			    VerifyData(&data, num, key_type);
			  }
		  }
		else if(status > 0)
		  {
			/* item not found */
			if(should_exist)
			  {
				ReportError("Item not found but should be: %ld", num);
			  }
		  }
		else
		  {
			/* database error */
			ReportError("Database error");
			return(-1);
		  }
			
	  }

	TraceMe(1, ("Correctly verified: %ld to %ld", low, high));

	return(0);

}

DBT *
GenData(int32 num)
{
	int32 n;
	static DBT *data=0;
	int32 *int32_array;
	int32 size;

	if(!data)
	  {
		data = (DBT*)malloc(sizeof(DBT));
		data->size = 0;
		data->data = 0;
	  }
	else if(data->data)
	  {
		free(data->data);
	  }

	n = rand();

	n = n % 512;  /* bound to a 2K size */

	
	size = sizeof(int32)*(n+1);
	int32_array = (int32 *) malloc((size_t)size);

	memcpy(&int32_array[0], &n, sizeof(int32));

	for(; n > 0; n--)
	  {
		memcpy(&int32_array[n], &num, sizeof(int32));
	  }

	data->data = (void*)int32_array;
	data->size = size;

	return(data);
}

#define ADD_RANGE 1
#define DELETE_RANGE 2

int
AddOrDelRange(int32 low, int32 high, int action, key_type_enum key_type)
{
	DBT *key, *data;
#if 0 /* only do this if your really analy checking the puts */
	DBT tmp_data;
#endif 
	int32 num;
	int status;

	if(action != ADD_RANGE && action != DELETE_RANGE)
		assert(0);

	if(action == ADD_RANGE)
	  {
		TraceMe(1, ("Adding: %ld to %ld: %s keys", low, high,
		    	key_type == USE_SMALL_KEY ? "SMALL" : "LARGE"));
	  }
	else
	  {
		TraceMe(1, ("Deleting: %ld to %ld: %s keys", low, high,
		    	key_type == USE_SMALL_KEY ? "SMALL" : "LARGE"));
	  }

	for(num = low; num <= high; num++)
	  {

		key = GenKey(num, key_type);

		if(action == ADD_RANGE)
		  {
			data = GenData(num);
			status = (*database->put)(database, key, data, 0);
		  }
		else
		  {
			status = (*database->del)(database, key, 0);
		  }

		if(status < 0)
		  {
			ReportError("Database error %s item: %ld",
							action == ADD_RANGE ? "ADDING" : "DELETING", 
							num);
		  }
		else if(status > 0)
		  {
			ReportError("Could not %s item: %ld", 
							action == ADD_RANGE ? "ADD" : "DELETE", 
							num);
		  }
		else if(action == ADD_RANGE)
		  {
#define SYNC_EVERY_TIME
#ifdef SYNC_EVERY_TIME
			status = (*database->sync)(database, 0);
			if(status != 0)
				ReportError("Database error syncing after add");
#endif

#if 0 /* only do this if your really analy checking the puts */
	 
			/* make sure we can still get it
			 */
			status = (*database->get)(database, key, &tmp_data, 0);

			if(status != 0)
			  {
				ReportError("Database error checking item just added: %d",
							num);
			  }
			else
			  {
				/* now verify that none of the ones we already
				 * put in have disappeared
				 */
				VerifyRange(low, num, SHOULD_EXIST, key_type);
			  }
#endif
			
		  }
	  }


	if(action == ADD_RANGE)
	  {
		TraceMe(1, ("Successfully added: %ld to %ld", low, high));
	  }
	else
	  {
		TraceMe(1, ("Successfully deleted: %ld to %ld", low, high));
	  }

	return(0);
}

int
TestRange(int32 low, int32 range, key_type_enum key_type)
{
	int status; int32 low_of_range1, high_of_range1; int32 low_of_range2, high_of_range2;
	int32 low_of_range3, high_of_range3;

	status = AddOrDelRange(low, low+range, ADD_RANGE, key_type);
	status = VerifyRange(low, low+range, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 1"));

	SeqDatabase();

	low_of_range1 = low;
	high_of_range1 = low+(range/2);
	low_of_range2 = high_of_range1+1;
	high_of_range2 = low+range;
	status = AddOrDelRange(low_of_range1, high_of_range1, DELETE_RANGE, key_type);
	status = VerifyRange(low_of_range1, high_of_range1, SHOULD_NOT_EXIST, key_type);
	status = VerifyRange(low_of_range2, low_of_range2, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 2"));

	SeqDatabase();

	status = AddOrDelRange(low_of_range1, high_of_range1, ADD_RANGE, key_type);
	/* the whole thing should exist now */
	status = VerifyRange(low, low+range, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 3"));

	SeqDatabase();

	status = AddOrDelRange(low_of_range2, high_of_range2, DELETE_RANGE, key_type);
	status = VerifyRange(low_of_range1, high_of_range1, SHOULD_EXIST, key_type);
	status = VerifyRange(low_of_range2, high_of_range2, SHOULD_NOT_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 4"));

	SeqDatabase();

	status = AddOrDelRange(low_of_range2, high_of_range2, ADD_RANGE, key_type);
	/* the whole thing should exist now */
	status = VerifyRange(low, low+range, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 5"));

	SeqDatabase();

	low_of_range1 = low;
	high_of_range1 = low+(range/3);
	low_of_range2 = high_of_range1+1;
	high_of_range2 = high_of_range1+(range/3);
	low_of_range3 = high_of_range2+1;
	high_of_range3 = low+range;
	/* delete range 2 */
	status = AddOrDelRange(low_of_range2, high_of_range2, DELETE_RANGE, key_type);
	status = VerifyRange(low_of_range1, high_of_range1, SHOULD_EXIST, key_type);
	status = VerifyRange(low_of_range2, low_of_range2, SHOULD_NOT_EXIST, key_type);
	status = VerifyRange(low_of_range3, low_of_range2, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 6"));

	SeqDatabase();

	status = AddOrDelRange(low_of_range2, high_of_range2, ADD_RANGE, key_type);
	/* the whole thing should exist now */
	status = VerifyRange(low, low+range, SHOULD_EXIST, key_type);

	TraceMe(1, ("Finished with sub test 7"));

	return(0);
}

#define START_RANGE 109876
int
main(int argc, char **argv)
{
	int32 i, j=0;
    int quick_exit = 0;
    int large_keys = 0;
    HASHINFO hash_info = {
        16*1024,
        0,
        0,
        0,
        0,
        0};


    if(argc > 1)
      {
        while(argc > 1)
	  {
            if(!strcmp(argv[argc-1], "-quick"))
                quick_exit = 1;
            else if(!strcmp(argv[argc-1], "-large"))
			  {
                large_keys = 1;
			  }
            argc--;
          }
      }

	database = dbopen("test.db", O_RDWR | O_CREAT, 0644, DB_HASH, &hash_info);

	if(!database)
	  {
		ReportError("Could not open database");
#ifdef unix
		perror("");
#endif
		exit(1);
	  }

	if(quick_exit)
	  {
		if(large_keys)
			TestRange(START_RANGE, 200, USE_LARGE_KEY);
		else
			TestRange(START_RANGE, 200, USE_SMALL_KEY);

		(*database->sync)(database, 0);
		(*database->close)(database);
        exit(0);
	  }

	for(i=100; i < 10000000; i+=200)
	  {
		if(1 || j)
		  {
			TestRange(START_RANGE, i, USE_LARGE_KEY);
			j = 0;
		  }
		else
		  {
			TestRange(START_RANGE, i, USE_SMALL_KEY);
			j = 1;
		  }

		if(1 == rand() % 3)
		  {
			(*database->sync)(database, 0);
		  }
		
		if(1 == rand() % 3)
	 	  {
			/* close and reopen */
			(*database->close)(database);
			database = dbopen("test.db", O_RDWR | O_CREAT, 0644, DB_HASH, 0);
			if(!database)
	  		{
				ReportError("Could not reopen database");
#ifdef unix
				perror("");
#endif
				exit(1);
	  		}
	 	  }
		else
		  {
			/* reopen database without closeing the other */
			database = dbopen("test.db", O_RDWR | O_CREAT, 0644, DB_HASH, 0);
			if(!database)
	  		{
				ReportError("Could not reopen database "
							"after not closing the other");
#ifdef unix
				perror("");
#endif
				exit(1);
	  		}
	 	  }
	  }

	return(0);
}
