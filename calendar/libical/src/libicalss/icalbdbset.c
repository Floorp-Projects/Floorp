/* -*- Mode: C -*-
  ======================================================================
  FILE: icalbdbset.c
 ======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icalbdbset.h"
#include "icalgauge.h"
#include <errno.h>
#include <sys/stat.h> /* for stat */
#include <stdio.h>

#ifndef WIN32
#include <unistd.h> /* for stat, getpid, unlink */
#include <fcntl.h> /* for fcntl */
#include <unistd.h> /* for fcntl */
#else
#define	S_IRUSR	S_IREAD		/* R for owner */
#define	S_IWUSR	S_IWRITE	/* W for owner */
#endif
#include <stdlib.h>
#include <string.h>

#include "icalbdbsetimpl.h"

#define STRBUF_LEN 255
#define MAX_RETRY 5

extern int errno;



/* these are just stub functions */
icalerrorenum icalbdbset_read_database(icalbdbset* bset, char *(*pfunc)(const DBT *dbt));
icalerrorenum icalbdbset_create_cluster(const char *path);
int icalbdbset_cget(DBC *dbcp, DBT *key, DBT *data, int access_method);

static int _compare_keys(DB *dbp, const DBT *a, const DBT *b);

    
/** Default options used when NULL is passed to icalset_new() **/
icalbdbset_options icalbdbset_options_default = {ICALBDB_EVENTS, DB_BTREE, 0644, 0, NULL, NULL};


static DB_ENV *ICAL_DB_ENV = 0;

/** Initialize the db environment */

int icalbdbset_init_dbenv(char *db_env_dir, void (*logDbFunc)(const char*, char*)) {
  int ret;
  int flags;

  if (db_env_dir) {
    struct stat env_dir_sb;
    
    if (stat(db_env_dir, &env_dir_sb)) {
      fprintf(stderr, "The directory '%s' is missing, please create it.\n", db_env_dir);
      return EINVAL;
    }
  }
  
  ret = db_env_create(&ICAL_DB_ENV, 0);

  if (ret) {
    /* some kind of error... */
    return ret;
  }

  /* Do deadlock detection internally */
  if ((ret = ICAL_DB_ENV->set_lk_detect(ICAL_DB_ENV, DB_LOCK_DEFAULT)) != 0) {
    char * foo = db_strerror(ret);
    fprintf(stderr, "Could not initialize the database locking environment\n");
    return ret;
  }    

  flags = DB_INIT_LOCK | DB_INIT_TXN | DB_CREATE | DB_THREAD | \
    DB_RECOVER | DB_INIT_LOG | DB_INIT_MPOOL;
  ret = ICAL_DB_ENV->open(ICAL_DB_ENV, db_env_dir,  flags, S_IRUSR|S_IWUSR);
  
  if (ret) {
    char * foo = db_strerror(ret);
    ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "dbenv->open");
    return ret;
  }

  /* display additional error messages */
  if (logDbFunc != NULL) {
     ICAL_DB_ENV->set_errcall(ICAL_DB_ENV, logDbFunc);
  }

  return ret;
}

void icalbdbset_checkpoint(void)
{
  int ret;
  char *err;

  switch (ret = ICAL_DB_ENV->txn_checkpoint(ICAL_DB_ENV, 0,0,0)) {
  case 0:
  case DB_INCOMPLETE:
    break;
  default:
    err = db_strerror(ret);
    ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "checkpoint failed");
    abort();
  }
}

void icalbdbset_rmdbLog(void)
{
    int      ret = 0;
    char**   listp;

    /* remove log files that are archivable (ie. no longer needed) */
    if (ICAL_DB_ENV->log_archive(ICAL_DB_ENV, &listp, DB_ARCH_ABS) == 0) {
       if (listp != NULL) {
          int ii = 0;
          while (listp[ii] != NULL) {
              ret = unlink(listp[ii]);
              ii++;
          }
          free(listp);
       }
    }
}

int icalbdbset_cleanup(void)
{
  int ret = 0;

  /* one last checkpoint.. */
  icalbdbset_checkpoint();

    /* remove logs that are not needed anymore */
    icalbdbset_rmdbLog();

  if (ICAL_DB_ENV) 
     ret = ICAL_DB_ENV->close(ICAL_DB_ENV, 0);

  return ret;
}

DB_ENV *icalbdbset_get_env(void) {
  return ICAL_DB_ENV;
}


/** Initialize an icalbdbset.  Also attempts to populate from the
 *  database (primary if only dbp is given, secondary if sdbp is
 *  given) and creates an empty object if retrieval is unsuccessful.
 *  pfunc is used to unpack data from the database.  If not given, we
 *  assume data is a string.
 */

icalset* icalbdbset_init(icalset* set, const char* dsn, void* options_in)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalbdbset_options *options = options_in;
    int ret;
    DB *cal_db;
    char *subdb_name;

    if (options == NULL) 
      *options = icalbdbset_options_default;

    switch (options->subdb) {
    case ICALBDB_CALENDARS:
      subdb_name = "calendars";
      break;
    case ICALBDB_EVENTS:
      subdb_name = "events";
      break;
    case ICALBDB_TODOS:
      subdb_name = "todos";
      break;
    case ICALBDB_REMINDERS:
      subdb_name = "reminders";
      break;
    }
  
    cal_db = icalbdbset_bdb_open(set->dsn, 
				 subdb_name, 
				 options->dbtype, 
				 options->mode,
				 options->flag);
    if (cal_db == NULL)
      return NULL;

    bset->dbp = cal_db;
    bset->sdbp = NULL;
    bset->gauge = 0;
    bset->cluster = 0;
  
    if ((ret = icalbdbset_read_database(bset, options->pfunc)) != ICAL_NO_ERROR) {
      return NULL;
    }

    return (icalset *)bset;
}


/** open a database and return a reference to it.  Used only for
    opening the primary index.
    flag = set_flag() DUP | DUP_SORT
 */

icalset* icalbdbset_new(const char* database_filename, 
			icalbdbset_subdb_type subdb_type,
			int dbtype, int flag)
{
  icalbdbset_options options = icalbdbset_options_default;

  options.subdb = subdb_type;
  options.dbtype = dbtype;
  options.flag = flag;
  
  /* this will in turn call icalbdbset_init */
  return icalset_new(ICAL_BDB_SET, database_filename, &options);
}

/**
 *  Open a secondary database, used for accessing secondary indices.
 *  The callback function tells icalbdbset how to associate secondary
 *  key information with primary data.  See the BerkeleyDB reference
 *  guide for more information.
 */

DB * icalbdbset_bdb_open_secondary(DB *dbp,
			       const char *database,
			       const char *sub_database, 
			       int (*callback) (DB *db, 
						const DBT *dbt1, 
						const DBT *dbt2, 
						DBT *dbt3),
			       int type)
{
  int ret;
  int flags;
    DB  *sdbp = NULL;

  if (!sub_database) 
        return NULL;

  if (!ICAL_DB_ENV)
        icalbdbset_init_dbenv(NULL, NULL);

  /* Open/create secondary */
  if((ret = db_create(&sdbp, ICAL_DB_ENV, 0)) != 0) {
        ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "secondary index: %s", sub_database);
        return NULL;
  }

  if ((ret = sdbp->set_flags(sdbp, DB_DUP | DB_DUPSORT)) != 0) {
        ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "set_flags error for secondary index: %s", sub_database);
        return NULL;
  }

  flags = DB_CREATE  | DB_THREAD;
  if ((ret = sdbp->open(sdbp, database, sub_database, type, (u_int32_t) flags, 0644)) != 0) {
        ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "failed to open secondary index: %s", sub_database);
        if (ret == DB_RUNRECOVERY)
            abort();
        else
            return NULL;
  }

  /* Associate the primary index with a secondary */
  if((ret = dbp->associate(dbp, sdbp, callback, 0)) != 0) {
        ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "failed to associate secondary index: %s", sub_database);
        return NULL;
  }

  return sdbp;
}

DB* icalbdbset_bdb_open(const char* path, 
				  const char *subdb, 
				int dbtype, 
				mode_t mode,
				int flag)
{
    DB  *dbp = NULL;
  int ret;
  int flags;

  /* Initialize the correct set of db subsystems (see capdb.c) */
  flags =  DB_CREATE | DB_THREAD;

  /* should just abort here instead of opening an env in the current dir..  */
  if (!ICAL_DB_ENV)
        icalbdbset_init_dbenv(NULL, NULL);

  /* Create and initialize database object, open the database. */
  if ((ret = db_create(&dbp, ICAL_DB_ENV, 0)) != 0) {
        return (NULL);
  }

  /* set comparison function, if BTREE */
  if (dbtype == DB_BTREE)
    dbp->set_bt_compare(dbp, _compare_keys);

  /* set DUP, DUPSORT */
  if (flag != 0)
    dbp->set_flags(dbp, flag);

  if ((ret = dbp->open(dbp, path, subdb, dbtype, flags, mode)) != 0) {
        ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "%s (database: %s): open failed.", path, subdb);
        if (ret == DB_RUNRECOVERY)
    abort();
        else
            return NULL;
  }

  return (dbp);
}

    
/* icalbdbset_parse_data -- parses using pfunc to unpack data. */
char *icalbdbset_parse_data(DBT *dbt, char *(*pfunc)(const DBT *dbt)) 
{
  char *ret;

  if(pfunc) {
    ret = (char *)pfunc(dbt);
  } else {
    ret = (char *) dbt->data;
  }

  return (ret);
}

/* This populates a cluster with the entire contents of a database */
icalerrorenum icalbdbset_read_database(icalbdbset* bset, char *(*pfunc)(const DBT *dbt))
{

  DB *dbp;
  DBC *dbcp;
  DBT key, data;
  char *str, *szpstr;
  int ret;
  char keystore[256];
  char datastore[1024];
  char *more_mem = NULL;
  DB_TXN *tid;

  memset(&key, 0, sizeof(DBT));
  memset(&data, 0, sizeof(DBT));

  if (bset->sdbp) { dbp = bset->sdbp; }
  else { dbp = bset->dbp; }
     
  if(!dbp) { goto err1; }

  bset->cluster = icalcomponent_new(ICAL_XROOT_COMPONENT);

  if ((ret = ICAL_DB_ENV->txn_begin(ICAL_DB_ENV, NULL, &tid, 0)) != 0) {
	char *foo = db_strerror(ret);
	abort();
  }

  /* acquire a cursor for the database */
  if ((ret = dbp->cursor(dbp, tid, &dbcp, 0)) != 0) {
    dbp->err(dbp, ret, "primary index");
    goto err1;
  }

  key.flags = DB_DBT_USERMEM;
  key.data = keystore;
  key.ulen = sizeof(keystore);

  data.flags= DB_DBT_USERMEM;
  data.data = datastore;
  data.ulen = sizeof(datastore);


  /* fetch the key/data pair */
  while (1) {
    ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
    if (ret == DB_NOTFOUND) {
      break;
    } else if (ret == ENOMEM) {
      if (more_mem) free (more_mem);
      more_mem = malloc(data.ulen+1024);
      data.data = more_mem;
      data.ulen = data.ulen+1024;
    } else if (ret == DB_LOCK_DEADLOCK) {
      char *foo = db_strerror(ret);
      abort(); /* should retry in case of DB_LOCK_DEADLOCK */
    } else if (ret) {
      char *foo = db_strerror(ret);
      /* some other weird-ass error  */
      dbp->err(dbp, ret, "cursor");
      abort();
    } else {
      icalcomponent *cl;
      
      /* this prevents an array read bounds error */
      if((str = (char *)calloc(data.size + 1, sizeof(char)))==NULL)
	goto err2;
      memcpy(str, (char *)data.data, data.size);
      
      cl = icalparser_parse_string(str);
      
      icalcomponent_add_component(bset->cluster, cl);
      free(str);
    }
  }
  if(ret != DB_NOTFOUND) {
      goto err2;
  }


  if (more_mem) {
      free(more_mem);
      more_mem = NULL;
  }

  if ((ret = dbcp->c_close(dbcp)) != 0) {
        char * foo = db_strerror(ret);
        abort(); /* should retry in case of DB_LOCK_DEADLOCK */
  }

    if ((ret = tid->commit(tid, 0)) != 0) {
        char * foo = db_strerror(ret);
        abort();
    }

  return ICAL_NO_ERROR;

 err2:
  if (more_mem) free(more_mem);
  dbcp->c_close(dbcp);
  abort(); /* should retry in case of DB_LOCK_DEADLOCK */
  return ICAL_INTERNAL_ERROR;

 err1:
  dbp->err(dbp, ret, "cursor index");
  abort();
  return (ICAL_FILE_ERROR);
}


/* XXX add more to this */
void icalbdbset_free(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    int ret;

    icalerror_check_arg_rv((bset!=0),"bset");

    if (bset->cluster != 0){
	icalbdbset_commit(set);
	icalcomponent_free(bset->cluster);
	bset->cluster=0;
    }

    if(bset->gauge !=0){
	icalgauge_free(bset->gauge);
    }

    if(bset->path != 0){
	free((char *)bset->path);
	bset->path = 0;
    }

    if(bset->sindex != 0) {
      free((char *)bset->sindex);
      bset->sindex = 0;
    }

    if (bset->dbp && 
	((ret = bset->dbp->close(bset->dbp, 0)) != 0)) {
    }
    bset->dbp = NULL;
}

/* return cursor is in rdbcp */
int icalbdbset_acquire_cursor(DB *dbp, DB_TXN *tid, DBC **rdbcp) {
  int ret=0;

  if((ret = dbp->cursor(dbp, tid, rdbcp, 0)) != 0) {
    dbp->err(dbp, ret, "couldn't open cursor");
    goto err1;
  }

  return ICAL_NO_ERROR;

 err1:
  return ICAL_FILE_ERROR;

}

/* returns key/data in arguments */
int icalbdbset_get_first(DBC *dbcp, DBT *key, DBT *data) {
  return icalbdbset_cget(dbcp, key, data, DB_FIRST);
}

int icalbdbset_get_next(DBC *dbcp, DBT *key, DBT *data) {
  return icalbdbset_cget(dbcp, key, data, DB_NEXT);
}

int icalbdbset_get_last(DBC *dbcp, DBT *key, DBT *data) {
  return icalbdbset_cget(dbcp, key, data, DB_LAST);
}

int icalbdbset_get_key(DBC *dbcp, DBT *key, DBT *data) {
  return icalbdbset_cget(dbcp, key, data, DB_SET);
}

int icalbdbset_delete(DB *dbp, DBT *key) {
  DB_TXN *tid;
  int ret;
    int done = 0;
    int retry = 0;

    while ((retry < MAX_RETRY) && !done) {

  if ((ret = ICAL_DB_ENV->txn_begin(ICAL_DB_ENV, NULL, &tid, 0)) != 0) {
            if (ret == DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else {
	char *foo = db_strerror(ret);
	abort();
  }
        }

        if ((ret = dbp->del(dbp, tid, key, 0)) != 0) {
            if (ret == DB_NOTFOUND) {
                /* do nothing - not an error condition */
            }
            else if (ret == DB_LOCK_DEADLOCK) {
                tid->abort(tid);
                retry++;
                continue;
            }
            else {
                char *strError = db_strerror(ret);
                icalerror_warn("icalbdbset_delete faild: ");
                icalerror_warn(strError);
                tid->abort(tid);
                return ICAL_FILE_ERROR;
            }
  }

  if ((ret = tid->commit(tid, 0)) != 0) {
            if (ret == DB_LOCK_DEADLOCK) {
                tid->abort(tid);
                retry++;
                continue;
            }
            else {
        char * foo = db_strerror(ret);
        abort();
  }
       }

       done = 1;   /* all is well */
    }

    if (!done) {
        if (tid != NULL) tid->abort(tid);
    }

  return ret;
}

int icalbdbset_cget(DBC *dbcp, DBT *key, DBT *data, int access_method) {
  int ret=0;

  key->flags |= DB_DBT_MALLOC; /* change these to DB_DBT_USERMEM */
  data->flags |= DB_DBT_MALLOC;

  /* fetch the key/data pair */
  if((ret = dbcp->c_get(dbcp, key, data, access_method)) != 0) {
    goto err1;
  }

  return ICAL_NO_ERROR;

 err1:
  return ICAL_FILE_ERROR;
}


int icalbdbset_cput(DBC *dbcp, DBT *key, DBT *data, int access_method) {
  int ret=0;

  key->flags |= DB_DBT_MALLOC; /* change these to DB_DBT_USERMEM */
  data->flags |= DB_DBT_MALLOC;

  /* fetch the key/data pair */
  if((ret = dbcp->c_put(dbcp, key, data, 0)) != 0) {
    goto err1;
  }

  return ICAL_NO_ERROR;

 err1:
  return ICAL_FILE_ERROR;
}


int icalbdbset_put(DB *dbp, DBT *key, DBT *data, int access_method)
{
    int     ret   = 0;
    DB_TXN *tid   = NULL;
    int     retry = 0;
    int     done  = 0;

    while ((retry < MAX_RETRY) && !done) {

  if ((ret = ICAL_DB_ENV->txn_begin(ICAL_DB_ENV, NULL, &tid, 0)) != 0) {
            if (ret == DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else {
	char *foo = db_strerror(ret);
	abort();
  }
        }
  
        if ((ret = dbp->put(dbp, tid, key, data, access_method)) != 0) {
            if (ret == DB_LOCK_DEADLOCK) {
                tid->abort(tid);
                retry++;
                continue;
            }
            else {
    char *strError = db_strerror(ret);
    icalerror_warn("icalbdbset_put faild: ");
    icalerror_warn(strError);
    tid->abort(tid);
    return ICAL_FILE_ERROR;
  }
        }

  if ((ret = tid->commit(tid, 0)) != 0) {
            if (ret == DB_LOCK_DEADLOCK) {
                tid->abort(tid);
                retry++;
                continue;
            }
            else {
        char * foo = db_strerror(ret);
        abort();
  }
       }

       done = 1;   /* all is well */
    }

    if (!done) {
        if (tid != NULL) tid->abort(tid);
        return ICAL_FILE_ERROR;
    }
    else
  return ICAL_NO_ERROR;
}

int icalbdbset_get(DB *dbp, DB_TXN *tid, DBT *key, DBT *data, int flags)
{
    return (dbp->get(dbp, tid, key, data, flags));
}

/** Return the path of the database file **/

const char* icalbdbset_path(icalset* set)
{
    icalerror_check_arg_rz((set!=0),"set");

    return set->dsn;
}

const char* icalbdbset_subdb(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rz((bset!=0),"bset");

    return bset->subdb;
}


/** Write changes out to the database file.
 */

icalerrorenum icalbdbset_commit(icalset *set) {
  DB *dbp;
  DBC *dbcp;
  DBT key, data;
  icalcomponent *c;
  char *str;
  int ret=0;
    int           reterr = ICAL_NO_ERROR;
  char keystore[256];
    char          uidbuf[256];
  char datastore[1024];
  char *more_mem = NULL;
    DB_TXN        *tid = NULL;
  icalbdbset *bset = (icalbdbset*)set;
  int bad_uid_counter = 0;
    int           retry = 0, done = 0, completed = 0, deadlocked = 0;

  icalerror_check_arg_re((bset!=0),"bset",ICAL_BADARG_ERROR);  

  dbp = bset->dbp;
  icalerror_check_arg_re((dbp!=0),"dbp is invalid",ICAL_BADARG_ERROR);

  if (bset->changed == 0)
    return ICAL_NO_ERROR;

  memset(&key, 0, sizeof(key));
  memset(&data, 0, sizeof(data));

  key.flags = DB_DBT_USERMEM; 
  key.data = keystore;
  key.ulen = sizeof(keystore);

  data.flags = DB_DBT_USERMEM;
  data.data = datastore;
  data.ulen = sizeof(datastore);
  
  if (!ICAL_DB_ENV)
        icalbdbset_init_dbenv(NULL, NULL);

    while ((retry < MAX_RETRY) && !done) {

  if ((ret = ICAL_DB_ENV->txn_begin(ICAL_DB_ENV, NULL, &tid, 0)) != 0) {
            if (ret ==  DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else if (ret == DB_RUNRECOVERY) {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "icalbdbset_commit: txn_begin failed");
	abort();
  }
            else {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "icalbdbset_commit");
                return ICAL_INTERNAL_ERROR;
            }
        }

  /* first delete everything in the database, because there could be removed components */
  if ((ret = dbp->cursor(dbp, tid, &dbcp, DB_DIRTY_READ)) != 0) {
            tid->abort(tid);
            if (ret == DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else if (ret == DB_RUNRECOVERY) {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "curor failed");
    abort();
            }
            else {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "curor failed");
    /* leave bset->changed set to true */
    return ICAL_INTERNAL_ERROR;
  }
        }

  /* fetch the key/data pair, then delete it */
        completed = 0;
        while (!completed && !deadlocked) {
    ret = dbcp->c_get(dbcp, &key, &data, DB_NEXT);
    if (ret == DB_NOTFOUND) {
                completed = 1;
    } else if (ret == ENOMEM)  {
      if (more_mem) free(more_mem);
      more_mem = malloc(data.ulen+1024);
      data.data = more_mem;
      data.ulen = data.ulen+1024;
    } else if (ret == DB_LOCK_DEADLOCK) {
                deadlocked = 1;
            } else if (ret == DB_RUNRECOVERY) {
                tid->abort(tid);
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "c_get failed.");
      abort();
            } else if (ret == 0) {
       if ((ret = dbcp->c_del(dbcp,0))!=0) {
         dbp->err(dbp, ret, "cursor");
         if (ret == DB_KEYEMPTY) {
	    /* never actually created, continue onward.. */
                        /* do nothing - break; */
         } else if (ret == DB_LOCK_DEADLOCK) {
                        deadlocked = 1;
         } else {
            char *foo = db_strerror(ret);
            abort();
         }
       }
            } else {  /* some other non-fatal error */
                dbcp->c_close(dbcp);
                tid->abort(tid);
                if (more_mem) {
                    free(more_mem);
                    more_mem = NULL;
                }
                return ICAL_INTERNAL_ERROR;
    }
  }

  if (more_mem) {
	  free(more_mem);
	  more_mem = NULL;
  }

        if (deadlocked) {
            dbcp->c_close(dbcp);
            tid->abort(tid);
            retry++;
            continue;  /* next retry */
        }

        deadlocked = 0;
        for (c = icalcomponent_get_first_component(bset->cluster,ICAL_ANY_COMPONENT);
             c != 0 && !deadlocked;
      c = icalcomponent_get_next_component(bset->cluster,ICAL_ANY_COMPONENT)) {

            memset(&key, 0, sizeof(key));
            memset(&data, 0, sizeof(data));

    /* Note that we're always inserting into a primary index. */
    if (icalcomponent_isa(c) != ICAL_VAGENDA_COMPONENT)  {
      char *uidstr = (char *)icalcomponent_get_uid(c);
                if (!uidstr) {   /* this shouldn't happen */
                    /* no uid string, we need to add one */
                    snprintf(uidbuf, 256, "baduid%d-%d", getpid(), bad_uid_counter++);
                    key.data = uidbuf;
      } else {
                    key.data = uidstr;
      }
    } else {
      char *relcalid = NULL;
      relcalid = (char*)icalcomponent_get_relcalid(c);
      if (relcalid == NULL) {
                    snprintf(uidbuf, 256, "baduid%d-%d", getpid(), bad_uid_counter++);
                    key.data = uidbuf;
      } else {
                    key.data = relcalid;
      }
    }
    key.size = strlen(key.data);

            str = icalcomponent_as_ical_string(c);
            data.data = str;
    data.size = strlen(str);

    if ((ret = dbcp->c_put(dbcp, &key, &data, DB_KEYLAST)) != 0) {
                if (ret == DB_LOCK_DEADLOCK) {
                    deadlocked = 1;
                }
                else if (ret == DB_RUNRECOVERY) {
                    ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "c_put failed.");
                    abort();
                }
                else {
                    ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "c_put failed %s.", str);
                    /* continue to try to put as many icalcomponent as possible */
                    reterr = ICAL_INTERNAL_ERROR;
                }
            }
    }

        if (deadlocked) {
            dbcp->c_close(dbcp);
            tid->abort(tid);
            retry++;
            continue;
  } 

  if ((ret = dbcp->c_close(dbcp)) != 0) {
            tid->abort(tid);
            if (ret == DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else if (ret == DB_RUNRECOVERY) {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "c_closed failed.");
                abort();
            }
            else {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "c_closed failed.");
                reterr = ICAL_INTERNAL_ERROR;
            }
  }

  if ((ret = tid->commit(tid, 0)) != 0) {
            tid->abort(tid);
            if (ret == DB_LOCK_DEADLOCK) {
                retry++;
                continue;
            }
            else if (ret == DB_RUNRECOVERY) {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "commit failed.");
    abort();
  }
            else {
                ICAL_DB_ENV->err(ICAL_DB_ENV, ret, "commit failed.");
                reterr = ICAL_INTERNAL_ERROR;
            }
        }

        done = 1;
    }

  bset->changed = 0;    
    return reterr;
} 


void icalbdbset_mark(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rv((bset!=0),"bset");

    bset->changed = 1;
}


icalcomponent* icalbdbset_get_component(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rz((bset!=0),"bset");

    return bset->cluster;
}


/* manipulate the components in the cluster */

icalerrorenum icalbdbset_add_component(icalset *set,
					icalcomponent* child)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_re((bset!=0),"bset", ICAL_BADARG_ERROR);
    icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

    icalcomponent_add_component(bset->cluster,child);

    icalbdbset_mark(set);

    return ICAL_NO_ERROR;
}


icalerrorenum icalbdbset_remove_component(icalset *set,
					   icalcomponent* child)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_re((bset!=0),"bset", ICAL_BADARG_ERROR);
    icalerror_check_arg_re((child!=0),"child",ICAL_BADARG_ERROR);

    icalcomponent_remove_component(bset->cluster,child);

    icalbdbset_mark(set);

    return ICAL_NO_ERROR;
}


int icalbdbset_count_components(icalset *set,
				 icalcomponent_kind kind)
{
    icalbdbset *bset = (icalbdbset*)set;

    if(set == 0){
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return -1;
    }

    return icalcomponent_count_components(bset->cluster,kind);
}


/** Set the gauge **/

icalerrorenum icalbdbset_select(icalset* set, icalgauge* gauge)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_re((bset!=0),"bset", ICAL_BADARG_ERROR);
    icalerror_check_arg_re(gauge!=0,"gauge",ICAL_BADARG_ERROR);

    bset->gauge = gauge;

    return ICAL_NO_ERROR;
}

    
/** Clear the gauge **/

void icalbdbset_clear(icalset* set)
{
  icalbdbset *bset = (icalbdbset*)set;
  icalerror_check_arg_rv((bset!=0),"bset");
    
  bset->gauge = 0;
}


icalcomponent* icalbdbset_fetch(icalset* set, icalcomponent_kind kind, const char* uid)
{
    icalcompiter i;    
  icalbdbset *bset = (icalbdbset*)set;
  icalerror_check_arg_rz((bset!=0),"bset");
    
  for(i = icalcomponent_begin_component(bset->cluster, kind);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
	icalcomponent *this = icalcompiter_deref(&i);
	icalproperty  *p = NULL;
	const char    *this_uid = NULL;

	if (this != 0){
	    if (kind == ICAL_VAGENDA_COMPONENT) {
		p = icalcomponent_get_first_property(this,ICAL_RELCALID_PROPERTY);
		if (p != NULL) this_uid = icalproperty_get_relcalid(p);
	    } else {
		p = icalcomponent_get_first_property(this,ICAL_UID_PROPERTY);
		if (p != NULL) this_uid = icalproperty_get_uid(p);
	    }

	    if(this_uid==NULL){
		icalerror_warn("icalbdbset_fetch found a component with no UID");
		continue;
	    }

	    if (strcmp(uid,this_uid)==0){
		return this;
	    }
	}
    }

    return 0;
}


int icalbdbset_has_uid(icalset* store,const char* uid)
{
    assert(0); /* HACK, not implemented */
    return 0;
}


/******* support routines for icalbdbset_fetch_match *********/

struct icalbdbset_id {
    char* uid;
    char* recurrence_id;
    int sequence;
};

void icalbdbset_id_free(struct icalbdbset_id *id)
{
    if(id->recurrence_id != 0){
	free(id->recurrence_id);
    }

    if(id->uid != 0){
	free(id->uid);
    }

}

struct icalbdbset_id icalbdbset_get_id(icalcomponent* comp)
{

    icalcomponent *inner;
    struct icalbdbset_id id;
    icalproperty *p;

    inner = icalcomponent_get_first_real_component(comp);
    
    p = icalcomponent_get_first_property(inner, ICAL_UID_PROPERTY);

    assert(p!= 0);

    id.uid = strdup(icalproperty_get_uid(p));

    p = icalcomponent_get_first_property(inner, ICAL_SEQUENCE_PROPERTY);

    if(p == 0) {
	id.sequence = 0;
    } else { 
	id.sequence = icalproperty_get_sequence(p);
    }

    p = icalcomponent_get_first_property(inner, ICAL_RECURRENCEID_PROPERTY);

    if (p == 0){
	id.recurrence_id = 0;
    } else {
	icalvalue *v;
	v = icalproperty_get_value(p);
	id.recurrence_id = strdup(icalvalue_as_ical_string(v));

	assert(id.recurrence_id != 0);
    }

    return id;
}

/* Find the component that is related to the given
   component. Currently, it just matches based on UID and
   RECURRENCE-ID */

icalcomponent* icalbdbset_fetch_match(icalset* set, icalcomponent *comp)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalcompiter i;    
    struct icalbdbset_id comp_id, match_id;
    
    icalerror_check_arg_rz((bset!=0),"bset");
    comp_id = icalbdbset_get_id(comp);

    for(i = icalcomponent_begin_component(bset->cluster,ICAL_ANY_COMPONENT);
	icalcompiter_deref(&i)!= 0; icalcompiter_next(&i)){
	
	icalcomponent *match = icalcompiter_deref(&i);

	match_id = icalbdbset_get_id(match);

	if(strcmp(comp_id.uid, match_id.uid) == 0 &&
	   ( comp_id.recurrence_id ==0 || 
	     strcmp(comp_id.recurrence_id, match_id.recurrence_id) ==0 )){

	    /* HACK. What to do with SEQUENCE? */

	    icalbdbset_id_free(&match_id);
	    icalbdbset_id_free(&comp_id);
	    return match;
	    
	}
	
	icalbdbset_id_free(&match_id);
    }

    icalbdbset_id_free(&comp_id);
    return 0;

}


icalerrorenum icalbdbset_modify(icalset* set, icalcomponent *old,
				 icalcomponent *newc)
{
    assert(0); /* HACK, not implemented */
    return ICAL_NO_ERROR;
}

/* caller is responsible to cal icalbdbset_free_cluster first */
icalerrorenum  icalbdbset_set_cluster(icalset* set, icalcomponent* cluster)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rz((bset!=0),"bset"); 

    bset->cluster = cluster;
}

icalerrorenum  icalbdbset_free_cluster(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rz((bset!=0),"bset");

    if (bset->cluster != NULL) icalcomponent_free(bset->cluster);
}

icalcomponent* icalbdbset_get_cluster(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalerror_check_arg_rz((bset!=0),"bset");

    return (bset->cluster);
}


/** Iterate through components. */
icalcomponent* icalbdbset_get_current_component (icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;

    icalerror_check_arg_rz((bset!=0),"bset");

    return icalcomponent_get_current_component(bset->cluster);
}


icalcomponent* icalbdbset_get_first_component(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalcomponent *c=0;

    icalerror_check_arg_rz((bset!=0),"bset");

    do {
        if (c == 0)
	    c = icalcomponent_get_first_component(bset->cluster,
						  ICAL_ANY_COMPONENT);
        else
	    c = icalcomponent_get_next_component(bset->cluster,
						 ICAL_ANY_COMPONENT);

	     if(c != 0 && (bset->gauge == 0 ||
		      icalgauge_compare(bset->gauge,c) == 1)){
	    return c;
	}

      } while (c!=0);

    return 0;
}


icalsetiter icalbdbset_begin_component(icalset* set, icalcomponent_kind kind, icalgauge* gauge, const char* tzid)
{
    icalsetiter itr = icalsetiter_null;
    icalcomponent* comp = NULL;
    icalcompiter citr;
    icalbdbset *bset = (icalbdbset*) set;
    struct icaltimetype start, next, end;
    icalproperty *dtstart, *rrule, *prop, *due;
    struct icalrecurrencetype recur;
    icaltimezone *u_zone;
    int g = 0;
    int orig_time_was_utc = 0;

    icalerror_check_arg_re((set!=0), "set", icalsetiter_null);

    itr.gauge = gauge;
    itr.tzid = tzid;

    citr = icalcomponent_begin_component(bset->cluster, kind);
    comp = icalcompiter_deref(&citr);

    if (gauge == 0) {
        itr.iter = citr;
        return itr;
    }

    /* if there is a gauge, the first matched component is returned */
    while (comp != 0) {

        /* check if it is a recurring component and with guage expand, if so
         * we need to add recurrence-id property to the given component */
        rrule = icalcomponent_get_first_property(comp, ICAL_RRULE_PROPERTY);
        g = icalgauge_get_expand(gauge);

        if (rrule != 0
            && g == 1) {

            /* it is a recurring event */

            u_zone = icaltimezone_get_builtin_timezone(itr.tzid);

      	    /* use UTC, if that's all we have. */
            if (!u_zone)
                u_zone = icaltimezone_get_utc_timezone();


            recur = icalproperty_get_rrule(rrule);

            if (icalcomponent_isa(comp) == ICAL_VEVENT_COMPONENT) {
                dtstart = icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY);
                if (dtstart)
                    start = icalproperty_get_dtstart(dtstart);
            } else if (icalcomponent_isa(comp) == ICAL_VTODO_COMPONENT) {
                    due = icalcomponent_get_first_property(comp, ICAL_DUE_PROPERTY);
                    if (due)
                        start = icalproperty_get_due(due);
            }

            /* Convert to the user's timezone in order to be able to compare
             * the results from the rrule iterator. */
            if (icaltime_is_utc(start)) {
	        start = icaltime_convert_to_zone(start, u_zone);
                orig_time_was_utc = 1;
            }

            if (itr.last_component == NULL) {
                itr.ritr = icalrecur_iterator_new(recur, start);
                next = icalrecur_iterator_next(itr.ritr);
                itr.last_component = comp;
            }
            else {
                next = icalrecur_iterator_next(itr.ritr);
                if (icaltime_is_null_time(next)){
                    itr.last_component = NULL;
                    icalrecur_iterator_free(itr.ritr);
                    itr.ritr = NULL;
                    /* no matched occurence */
                    goto getNextComp;
                } else {
                    itr.last_component = comp;
                }
            }

            /* if it is excluded, do next one */
            if (icalproperty_recurrence_is_excluded(comp, &start, &next)) {
	        icalrecur_iterator_decrement_count(itr.ritr);
                continue;
            }

            /* add recurrence-id value to the property if the property already exist;
             * add the recurrence id property and the value if the property does not exist */
            prop = icalcomponent_get_first_property(comp, ICAL_RECURRENCEID_PROPERTY);
            if (prop == 0)
                icalcomponent_add_property(comp, icalproperty_new_recurrenceid(next));
            else
                icalproperty_set_recurrenceid(prop, next);

            /* convert the next recurrence time into the user's timezone */
            if (orig_time_was_utc) 
                next = icaltime_convert_to_zone(next, icaltimezone_get_utc_timezone());

        } /* end of a recurring event */

        if (gauge == 0 || icalgauge_compare(itr.gauge, comp) == 1) {
	    /* find a matched and return it */
            itr.iter = citr;
            return itr;
        }
       
        /* if it is a recurring but no matched occurrence has been found OR
         * it is not a recurring and no matched component has been found,
         * read the next component to find out */
getNextComp: 
        if ((rrule != NULL && itr.last_component == NULL) ||
           (rrule == NULL)) {
            comp =  icalcompiter_next(&citr);
            comp = icalcompiter_deref(&citr);
    }
    } /* while */

    /* no matched component has found */
    return icalsetiter_null;
}

icalcomponent* icalbdbset_form_a_matched_recurrence_component(icalsetiter* itr)
{
    icalcomponent* comp = NULL;
    struct icaltimetype start, next, end;
    icalproperty *dtstart, *rrule, *prop, *due;
    struct icalrecurrencetype recur;
    icaltimezone *u_zone;
    int g = 0;
    int orig_time_was_utc = 0;

    comp = itr->last_component;

    if (comp == NULL || itr->gauge == NULL) {
        return NULL;
    }


    rrule = icalcomponent_get_first_property(comp, ICAL_RRULE_PROPERTY);
    /* if there is no RRULE, simply return to the caller */
    if (rrule == NULL)
        return NULL;

    u_zone = icaltimezone_get_builtin_timezone(itr->tzid);

    /* use UTC, if that's all we have. */
    if (!u_zone)
        u_zone = icaltimezone_get_utc_timezone();

    recur = icalproperty_get_rrule(rrule);

    if (icalcomponent_isa(comp) == ICAL_VEVENT_COMPONENT) {
        dtstart = icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY);
        if (dtstart)
            start = icalproperty_get_dtstart(dtstart);
    } else if (icalcomponent_isa(comp) == ICAL_VTODO_COMPONENT) {
        due = icalcomponent_get_first_property(comp, ICAL_DUE_PROPERTY);
        if (due)
            start = icalproperty_get_due(due);
    }

    /* Convert to the user's timezone in order to be able to compare the results
     * from the rrule iterator. */
    if (icaltime_is_utc(start)) {
        start = icaltime_convert_to_zone(start, u_zone);
        orig_time_was_utc = 1;
    }

    if (itr->ritr == NULL) {
        itr->ritr = icalrecur_iterator_new(recur, start);
        next = icalrecur_iterator_next(itr->ritr);
        itr->last_component = comp;
    } else {
        next = icalrecur_iterator_next(itr->ritr);
        if (icaltime_is_null_time(next)){
            /* no more recurrence, returns */
            itr->last_component = NULL;
            icalrecur_iterator_free(itr->ritr);
            itr->ritr = NULL;
            /* no more pending matched occurence,
             * all the pending matched occurences have been returned */
            return NULL;
        } else {
            itr->last_component = comp;
        }
    }

    /* if it is excluded, return NULL to the caller */
    if (icalproperty_recurrence_is_excluded(comp, &start, &next)) {
        icalrecur_iterator_decrement_count(itr->ritr);
       return NULL; 
    }

    /* set recurrence-id value to the property if the property already exist;
     * add the recurrence id property and the value if the property does not exist */
    prop = icalcomponent_get_first_property(comp, ICAL_RECURRENCEID_PROPERTY);
    if (prop == 0)
        icalcomponent_add_property(comp, icalproperty_new_recurrenceid(next));
    else
        icalproperty_set_recurrenceid(prop, next);

    if (orig_time_was_utc) {
        next = icaltime_convert_to_zone(next, icaltimezone_get_utc_timezone());
    }


     if (itr->gauge == 0 || icalgauge_compare(itr->gauge, comp) == 1) {
         /* find a matched and return it */
         return comp;
     } 

     /* not matched */
     return NULL;

}

icalcomponent* icalbdbsetiter_to_next(icalset *set, icalsetiter* i)
{

    icalcomponent* comp = NULL;
    icalbdbset *bset = (icalbdbset*) set;
    struct icaltimetype start, next, end;
    icalproperty *dtstart, *rrule, *prop, *due;
    struct icalrecurrencetype recur;
    icaltimezone *u_zone;
    int g = 0;
    int orig_time_was_utc = 0;

    do {

        /* no pending occurence, read the next component */
        if (i->last_component == NULL) {
            comp = icalcompiter_next(&(i->iter));
        }
        else {
            comp = i->last_component;
        }

        /* no next component, simply return */
        if (comp == 0) return NULL;
        if (i->gauge == 0) return comp;

        /* finding the next matched component and return it to the caller */

        rrule = icalcomponent_get_first_property(comp, ICAL_RRULE_PROPERTY);
        g = icalgauge_get_expand(i->gauge);

        /* a recurring component with expand query */
        if (rrule != 0
            && g == 1) {

            u_zone = icaltimezone_get_builtin_timezone(i->tzid);

            /* use UTC, if that's all we have. */
            if (!u_zone)
                u_zone = icaltimezone_get_utc_timezone();

            recur = icalproperty_get_rrule(rrule);

            if (icalcomponent_isa(comp) == ICAL_VEVENT_COMPONENT) {
                dtstart = icalcomponent_get_first_property(comp, ICAL_DTSTART_PROPERTY);
                if (dtstart)
                    start = icalproperty_get_dtstart(dtstart);
            } else if (icalcomponent_isa(comp) == ICAL_VTODO_COMPONENT) {
                due = icalcomponent_get_first_property(comp, ICAL_DUE_PROPERTY);
                if (due)
                    start = icalproperty_get_due(due);
            }

            /* Convert to the user's timezone in order to be able to compare
             * the results from the rrule iterator. */
            if (icaltime_is_utc(start)) {
                start = icaltime_convert_to_zone(start, u_zone);
                orig_time_was_utc = 1;
            }

            if (i->ritr == NULL) {
                i->ritr = icalrecur_iterator_new(recur, start);
                next = icalrecur_iterator_next(i->ritr);
                i->last_component = comp;
            } else {
                next = icalrecur_iterator_next(i->ritr);
                if (icaltime_is_null_time(next)) {
                    i->last_component = NULL;
                    icalrecur_iterator_free(i->ritr);
                    i->ritr = NULL;
                    /* no more occurence, should go to get next component */
                    continue;
                } else {
                    i->last_component = comp;
                }
            }

            /* if it is excluded, do next one */
            if (icalproperty_recurrence_is_excluded(comp, &start, &next)) {
                icalrecur_iterator_decrement_count(i->ritr);
                continue;
            }

            /* set recurrence-id value to the property if the property already exist;
             * add the recurrence id property and the value if the property does not exist */
            prop = icalcomponent_get_first_property(comp, ICAL_RECURRENCEID_PROPERTY);
            if (prop == 0)
               icalcomponent_add_property(comp, icalproperty_new_recurrenceid(next));
            else
               icalproperty_set_recurrenceid(prop, next);

            if (orig_time_was_utc) {
                next = icaltime_convert_to_zone(next, icaltimezone_get_utc_timezone());
            }
 
        } /* end of recurring event with expand query */

        if(comp != 0 && (i->gauge == 0 ||
            icalgauge_compare(i->gauge, comp) == 1)){
           /* found a matched, return it */ 
            return comp;
        }
    } while (comp != 0);

    return 0;

}

icalcomponent* icalbdbset_get_next_component(icalset* set)
{
    icalbdbset *bset = (icalbdbset*)set;
    icalcomponent *c=0;

    struct icaltimetype start, next;
    icalproperty *dtstart, *rrule, *prop, *due;
    struct icalrecurrencetype recur;
    int g = 0;

    icalerror_check_arg_rz((bset!=0),"bset");
    
    do {
            c = icalcomponent_get_next_component(bset->cluster,
					     ICAL_ANY_COMPONENT);
                if(c != 0 && (bset->gauge == 0 ||
                    icalgauge_compare(bset->gauge,c) == 1)){
                    return c;
                }

    } while(c != 0);
    
    return 0;
}

int icalbdbset_begin_transaction(DB_TXN* parent_tid, DB_TXN** tid)
{
    return (ICAL_DB_ENV->txn_begin(ICAL_DB_ENV, parent_tid, tid, 0));
}

int icalbdbset_commit_transaction(DB_TXN* txnid)
{
    return (txnid->commit(txnid, 0));
}


static int _compare_keys(DB *dbp, const DBT *a, const DBT *b)
{
/* 
 * Returns: 
 * < 0 if a < b 
 * = 0 if a = b 
 * > 0 if a > b 
 */ 
	
    char*  ac = (char*)a->data;
    char*  bc = (char*)b->data;
    return (strncmp(ac, bc, a->size));
}



