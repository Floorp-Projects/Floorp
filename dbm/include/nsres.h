#ifndef NSRES_H
#define NSRES_H
#include "mcom_db.h"

__BEGIN_DECLS

/* C version */
#define NSRESHANDLE void *

typedef void (*NSRESTHREADFUNC)(void *);

typedef struct NSRESTHREADINFO
{
	void *lock;
	NSRESTHREADFUNC fn_lock;
	NSRESTHREADFUNC fn_unlock;
} NSRESTHREADINFO;

#define MAXBUFNUM 10
#define MAXSTRINGLEN 300

#define NSRES_CREATE 1
#define NSRES_OPEN 2



NSRESHANDLE NSResCreateTable(const char *filename, NSRESTHREADINFO *threadinfo);
NSRESHANDLE NSResOpenTable(const char *filename, NSRESTHREADINFO *threadinfo);
void NSResCloseTable(NSRESHANDLE handle);

char *NSResLoadString(NSRESHANDLE handle, const char * library, int32 id, 
	unsigned int charsetid, char *retbuf);
int32 NSResGetSize(NSRESHANDLE handle, const char *library, int32 id);
int32 NSResLoadResource(NSRESHANDLE handle, const char *library, int32 id, char *retbuf);
int NSResAddString(NSRESHANDLE handle, const char *library, int32 id, const char *string, unsigned int charset);

__END_DECLS


#endif

