/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalbdbset.h
 CREATOR: dml 12 December 2001
 (C) COPYRIGHT 2001, Critical Path

 $Id: icalbdbset.h,v 1.4 2002/09/26 22:24:52 lindner Exp $
 $Locker:  $
======================================================================*/

#ifndef ICALBDBSET_H
#define ICALBDBSET_H

#include "ical.h"
#include "icalset.h"
#include "icalgauge.h"
#include <db.h>

typedef struct icalbdbset_impl icalbdbset;

enum icalbdbset_subdb_type {ICALBDB_CALENDARS, ICALBDB_EVENTS, ICALBDB_TODOS, ICALBDB_REMINDERS};
typedef enum icalbdbset_subdb_type icalbdbset_subdb_type;

/** sets up the db environment, should be done in parent thread.. */
int icalbdbset_init_dbenv(char *db_env_dir, void (*logDbFunc)(const char*, char*));

icalset*  icalbdbset_init(icalset* set, const char *dsn, void *options);
int icalbdbset_cleanup(void);
void icalbdbset_checkpoint(void);
void icalbdbset_rmdbLog(void);

/** Creates a component handle.  flags allows caller to 
   specify if database is internally a BTREE or HASH */
icalset * icalbdbset_new(const char* database_filename, 
			 icalbdbset_subdb_type subdb_type,
			 int dbtype, int flag);

DB * icalbdbset_bdb_open_secondary(DB *dbp,
			       const char *subdb,
			       const char *sindex, 
			       int (*callback) (DB *db, 
						const DBT *dbt1, 
						const DBT *dbt2, 
						DBT *dbt3),
			       int type);
			       
char *icalbdbset_parse_data(DBT *dbt, char *(*pfunc)(const DBT *dbt)) ;

void icalbdbset_free(icalset* set);

/* cursor operations */
int icalbdbset_acquire_cursor(DB *dbp, DB_TXN* tid, DBC **rdbcp);
int icalbdbset_cget(DBC *dbcp, DBT *key, DBT *data, int access_method);
int icalbdbset_cput(DBC *dbcp, DBT *key, DBT *data, int access_method);

int icalbdbset_get_first(DBC *dbcp, DBT *key, DBT *data);
int icalbdbset_get_next(DBC *dbcp, DBT *key, DBT *data);
int icalbdbset_get_last(DBC *dbcp, DBT *key, DBT *data);
int icalbdbset_get_key(DBC *dbcp, DBT *key, DBT *data);
int icalbdbset_delete(DB *dbp, DBT *key);
int icalbdbset_put(DB *dbp, DBT *key, DBT *data, int access_method);
int icalbdbset_get(DB *dbp, DB_TXN *tid, DBT *key, DBT *data, int flags);

const char* icalbdbset_path(icalset* set);
const char* icalbdbset_subdb(icalset* set);

/* Mark the set as changed, so it will be written to disk when it
   is freed. Commit writes to disk immediately. */
void icalbdbset_mark(icalset* set);
icalerrorenum icalbdbset_commit(icalset *set);

icalerrorenum icalbdbset_add_component(icalset* set,
					icalcomponent* child);

icalerrorenum icalbdbset_remove_component(icalset* set,
					   icalcomponent* child);

int icalbdbset_count_components(icalset* set,
				 icalcomponent_kind kind);

/* Restrict the component returned by icalbdbset_first, _next to those
   that pass the gauge. _clear removes the gauge */
icalerrorenum icalbdbset_select(icalset* store, icalgauge* gauge);
void icalbdbset_clear(icalset* store);

/* Get and search for a component by uid */
icalcomponent* icalbdbset_fetch(icalset* set, icalcomponent_kind kind, const char* uid);
int icalbdbset_has_uid(icalset* set, const char* uid);
icalcomponent* icalbdbset_fetch_match(icalset* set, icalcomponent *c);


icalerrorenum icalbdbset_modify(icalset* set, icalcomponent *old,
				icalcomponent *newc);

/* cluster management functions */
icalerrorenum  icalbdbset_set_cluster(icalset* set, icalcomponent* cluster);
icalerrorenum  icalbdbset_free_cluster(icalset* set);
icalcomponent* icalbdbset_get_cluster(icalset* set);

/* Iterate through components. If a gauge has been defined, these
   will skip over components that do not pass the gauge */

icalcomponent* icalbdbset_get_current_component (icalset* set);
icalcomponent* icalbdbset_get_first_component(icalset* set);
icalcomponent* icalbdbset_get_next_component(icalset* set);

/* External iterator for thread safety */
icalsetiter icalbdbset_begin_component(icalset* set, icalcomponent_kind kind, icalgauge* gauge, const char* tzid);

icalcomponent* icalbdbset_form_a_matched_recurrence_component(icalsetiter* itr);

icalcomponent* icalbdbsetiter_to_next(icalset* set, icalsetiter* i);
icalcomponent* icalbdbsetiter_to_prior(icalset* set, icalsetiter* i);

/* Return a reference to the internal component. You probably should
   not be using this. */

icalcomponent* icalbdbset_get_component(icalset* set);

DB_ENV *icalbdbset_get_env(void);


int icalbdbset_begin_transaction(DB_TXN* parent_id, DB_TXN** txnid);
int icalbdbset_commit_transaction(DB_TXN* txnid);

DB* icalbdbset_bdb_open(const char* path, 
			const char *subdb,
			int type,
			mode_t mode, int flag);


typedef struct icalbdbset_options {
  icalbdbset_subdb_type subdb;	   /**< the subdatabase to open */
  int                   dbtype;	   /**< db_open type: DB_HASH | DB_BTREE */
  mode_t                mode;	   /**< file mode */
  u_int32_t             flag;      /**< DB->set_flags(): DB_DUP | DB_DUPSORT */
  char *(*pfunc)(const DBT *dbt);  /**< parsing function */
  int (*callback) (DB *db,	   /**< callback for secondary db open */
		   const DBT *dbt1, 
		   const DBT *dbt2, 
		   DBT *dbt3);
} icalbdbset_options;

#endif /* !ICALBDBSET_H */



