#include "watcomfx.h"

#include "nsres.h"

#include <stdio.h>

#if defined(BSDI)||defined(RHAPSODY)
#include <stdlib.h>
#else
#include <malloc.h>
#endif

#include <string.h>

struct RESDATABASE
{
	DB *hdb;
	NSRESTHREADINFO *threadinfo;
	char * pbuf[MAXBUFNUM];
} ;
typedef struct RESDATABASE *  RESHANDLE;

typedef struct STRINGDATA
{
	char *str;
	unsigned int charsetid;
} STRINGDATA;


typedef unsigned int CHARSETTYPE;
#define RES_LOCK	if (hres->threadinfo)   hres->threadinfo->fn_lock(hres->threadinfo->lock);
#define RES_UNLOCK  if (hres->threadinfo)   hres->threadinfo->fn_unlock(hres->threadinfo->lock);

int GenKeyData(const char *library, int32 id, DBT *key);

/* 
  Right now, the page size used for resource is same as for Navigator cache
  database
 */
HASHINFO res_hash_info = {
        32*1024,
        0,
        0,
        0,
        0,   /* 64 * 1024U  */
        0};

int GenKeyData(const char *library, int32 id, DBT *key)
{
	char idstr[10];
	static char * strdata = NULL;
	int len;

	if (strdata)
		free (strdata);

	if (id == 0)
		idstr[0] = '\0';
	else
	{
		sprintf(idstr, "%d", id);
		/*	itoa(id, idstr, 10);  */
	}

	if (library == NULL)
		len = strlen(idstr) + 1;
	else
		len = strlen(library) + strlen(idstr) + 1;
	strdata = (char *) malloc (len);
	strcpy(strdata, library);
	strcat(strdata, idstr);

	key->size = len;
	key->data = strdata;

	return 1;
}

NSRESHANDLE NSResCreateTable(const char *filename, NSRESTHREADINFO *threadinfo)
{
	RESHANDLE hres;
	int flag;

	flag = O_RDWR | O_CREAT;

	hres = (RESHANDLE) malloc ( sizeof(struct RESDATABASE) );
	memset(hres, 0, sizeof(struct RESDATABASE));

	if (threadinfo && threadinfo->lock && threadinfo->fn_lock 
	  && threadinfo->fn_unlock)
	{
		hres->threadinfo = (NSRESTHREADINFO *) malloc( sizeof(NSRESTHREADINFO) );
		hres->threadinfo->lock = threadinfo->lock;
		hres->threadinfo->fn_lock = threadinfo->fn_lock;
		hres->threadinfo->fn_unlock = threadinfo->fn_unlock;
	}


	RES_LOCK

	hres->hdb = dbopen(filename, flag, 0644, DB_HASH, &res_hash_info);

	RES_UNLOCK

	if(!hres->hdb)
		return NULL;

	return (NSRESHANDLE) hres;
}

NSRESHANDLE NSResOpenTable(const char *filename, NSRESTHREADINFO *threadinfo)
{
	RESHANDLE hres;
	int flag;

	flag = O_RDONLY;  /* only open database for reading */

	hres = (RESHANDLE) malloc ( sizeof(struct RESDATABASE) );
	memset(hres, 0, sizeof(struct RESDATABASE));

	if (threadinfo && threadinfo->lock && threadinfo->fn_lock 
	  && threadinfo->fn_unlock)
	{
		hres->threadinfo = (NSRESTHREADINFO *) malloc( sizeof(NSRESTHREADINFO) );
		hres->threadinfo->lock = threadinfo->lock;
		hres->threadinfo->fn_lock = threadinfo->fn_lock;
		hres->threadinfo->fn_unlock = threadinfo->fn_unlock;
	}


	RES_LOCK

	hres->hdb = dbopen(filename, flag, 0644, DB_HASH, &res_hash_info);

	RES_UNLOCK

	if(!hres->hdb)
		return NULL;

	return (NSRESHANDLE) hres;
}



void NSResCloseTable(NSRESHANDLE handle)
{
	RESHANDLE hres;
	int i;

	if (handle == NULL)
		return;
	hres = (RESHANDLE) handle;

	RES_LOCK

	(*hres->hdb->sync)(hres->hdb, 0);
	(*hres->hdb->close)(hres->hdb);

	RES_UNLOCK

	for (i = 0; i < MAXBUFNUM; i++)
	{
		if (hres->pbuf[i])
			free (hres->pbuf[i]);
	}

	if (hres->threadinfo)
		free (hres->threadinfo);
	free (hres);
}


char *NSResLoadString(NSRESHANDLE handle, const char * library, int32 id, 
	unsigned int charsetid, char *retbuf)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return NULL;

	hres = (RESHANDLE) handle;
	GenKeyData(library, id, &key);

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	if (retbuf)
	{
		memcpy(retbuf, (char *)data.data + sizeof(CHARSETTYPE), data.size - sizeof(CHARSETTYPE));
		return retbuf;
	}
	else 
	{
		static int WhichString = 0;
		static int bFirstTime = 1;
		char *szLoadedString;
		int i;

		RES_LOCK

		if (bFirstTime) {
			for (i = 0; i < MAXBUFNUM; i++)
				hres->pbuf[i] = (char *) malloc(MAXSTRINGLEN * sizeof(char));
			bFirstTime = 0; 
		} 

		szLoadedString = hres->pbuf[WhichString];
		WhichString++;

		/* reset to 0, if WhichString reaches to the end */
		if (WhichString == MAXBUFNUM)  
			WhichString = 0;

		if (status == 0)
			memcpy(szLoadedString, (char *) data.data + sizeof(CHARSETTYPE), 
			  data.size - sizeof(CHARSETTYPE));
		else
			szLoadedString[0] = 0; 

		RES_UNLOCK

		return szLoadedString;
	}
}

int32 NSResGetSize(NSRESHANDLE handle, const char *library, int32 id)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;
	GenKeyData(library, id, &key);

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	return data.size - sizeof(CHARSETTYPE);
}

int32 NSResLoadResource(NSRESHANDLE handle, const char *library, int32 id, char *retbuf)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;
	GenKeyData(library, id, &key);

	RES_LOCK

	status = (*hres->hdb->get)(hres->hdb, &key, &data, 0);

	RES_UNLOCK

	if (retbuf)
	{
		memcpy(retbuf, (char *)data.data + sizeof(CHARSETTYPE), data.size - sizeof(CHARSETTYPE));
		return data.size;
	}
	else
		return 0;
}

int NSResAddString(NSRESHANDLE handle, const char *library, int32 id, 
  const char *string, unsigned int charset)
{
	int status;
	RESHANDLE hres;
	DBT key, data;
	char * recdata;

	if (handle == NULL)
		return 0;
	hres = (RESHANDLE) handle;

	GenKeyData(library, id, &key);

	data.size = sizeof(CHARSETTYPE) + (strlen(string) + 1) ;

	recdata = (char *) malloc(data.size) ;

	/* set charset to the first field of record data */
	*((CHARSETTYPE *)recdata) = (CHARSETTYPE)charset;

	/* set data field */
	memcpy(recdata+sizeof(CHARSETTYPE), string, strlen(string) + 1);

	data.data = recdata;

	RES_LOCK

	status = (*hres->hdb->put)(hres->hdb, &key, &data, 0);


	if (recdata)
		free(recdata);

	RES_UNLOCK

	return status;
}
