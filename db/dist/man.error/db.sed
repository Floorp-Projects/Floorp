# @(#)db.sed	8.6 (Sleepycat) 11/12/97
#
# Various corrections for the DB package.

# Translate macros that call functions.
s/^DB_ADDSTR$/__db_abspath\
strlen\
memcpy/
s/^DISCARD$/memp_fput\
lock_put/
s/^GET_META$/lock_get\
__ham_get_page\
lock_put/
s/^RELEASE_META$/lock_put\
__ham_put_page/
s/^DIRTY_META$/lock_get\
lock_put/

s/^BT_STK_ENTER$/__bam_stkgrow/
s/^DB_CHECK_FCOMBO$/__db_ferr/
s/^DB_CHECK_FLAGS$/__db_ferr/
s/^FREE$/free/
s/^FREES$/free/
s/^GETHANDLE$/__db_gethandle/
s/^H_GETHANDLE$/__db_gethandle/
s/^H_PUTHANDLE$/__db_puthandle/
s/^LOCKBUFFER$/__db_mutex_lock/
s/^LOCKBUFFER$/__db_mutex_lock/
s/^LOCKHANDLE$/__db_mutex_lock/
s/^LOCKINIT$/__db_mutex_init/
s/^LOCKREGION$/__db_mutex_lock/
s/^LOCKREGION$/__db_mutex_lock/
s/^LOCK_LOCKREGION$/__db_mutex_lock/
s/^LOCK_LOGREGION$/__db_mutex_lock/
s/^LOCK_LOGTHREAD$/__db_mutex_lock/
s/^LOCK_TXNREGION$/__db_mutex_lock/
s/^LOCK_TXNTHREAD$/__db_mutex_lock/
s/^PUTHANDLE$/__db_puthandle/
s/^PUT_HKEYDATA$/memcpy/
s/^UNLOCKBUFFER$/__db_mutex_unlock/
s/^UNLOCKBUFFER$/__db_mutex_unlock/
s/^UNLOCKHANDLE$/__db_mutex_unlock/
s/^UNLOCKREGION$/__db_mutex_unlock/
s/^UNLOCKREGION$/__db_mutex_unlock/
s/^UNLOCK_LOCKREGION$/__db_mutex_unlock/
s/^UNLOCK_LOGREGION$/__db_mutex_unlock/
s/^UNLOCK_LOGTHREAD$/__db_mutex_unlock/
s/^UNLOCK_TXNREGION$/__db_mutex_unlock/
s/^UNLOCK_TXNTHREAD$/__db_mutex_unlock/
s/^__BT_LPUT$/lock_put/
s/^__BT_TLPUT$/lock_put/

# Discard all other macro names.
/^[A-Z][A-Z0-9_]*$/d

# Per-OS spooge.
/^CloseHandle$/d
/^FSp2FullPath$/d
/^GetFileInformationByHandle$/d
/^Special2FSSpec$/d
/^UnmapViewOfFile$/d
/^_findclose$/d
/^_findfirst$/d
/^_findnext$/d
/^_get_osfhandle$/d
/^_lseeki64$/d
/^llseek$/d

# Db
/^db_errcall$/d
/^dbenv->db_errcall$/d
/^dbenv->yield$/d
/^dbmp->dbenv->db_yield$/d
/^dbmp->dbenv->yield$/d
/^yield$/d

/^dbp->db_malloc$/s/.*/malloc/
/^db_malloc$/s/.*/malloc/
s/^freefunc$/__bam_free\
__ham_del_page/
s/^newfunc$/__bam_new\
__ham_overflow_page/
s/^am_func$/__bam_bdup\
__ham_hdup/

s/^dbc->c_close$/xxxDBcursor->c_close/
s/^dbc->c_del$/xxxDBcursor->c_del/
s/^dbc->c_get$/xxxDBcursor->c_get/
s/^dbc->c_put$/xxxDBcursor->c_put/

s/^db->close$/xxxDB->close/
s/^db_close$/xxxDB->close/
s/^db->cursor$/xxxDB->cursor/
s/^db->del$/xxxDB->del/
s/^db->fd$/xxxDB->fd/
s/^db_fd$/xxxDB->fd/
s/^db->get$/xxxDB->get/
s/^db->put$/xxxDB->put/
s/^db->stat$/xxxDB->stat/
s/^db->sync$/xxxDB->sync/

s/^dbp->close$/xxxDB->close/
s/^dbp->cursor$/xxxDB->cursor/
s/^dbp->del$/xxxDB->del/
s/^dbp->fd$/xxxDB->fd/
s/^dbp->get$/xxxDB->get/
s/^dbp->put$/xxxDB->put/
s/^dbp->stat$/xxxDB->stat/
s/^dbp->sync$/xxxDB->sync/

# Btree.
/^t->bt_prefix$/d

s/^__bam_c_close$/xxxDBcursor->c_close/
s/^__bam_c_del$/xxxDBcursor->c_del/
s/^__bam_c_get$/xxxDBcursor->c_get/
s/^__bam_c_put$/xxxDBcursor->c_put/
s/^__bam_cursor$/xxxDB->cursor/
s/^__bam_delete$/xxxDB->del/
s/^__bam_get$/xxxDB->get/
s/^__bam_put$/xxxDB->put/
s/^__bam_stat$/xxxDB->stat/
s/^__bam_sync$/xxxDB->sync/

# Hash.
/^hashp->hash$/d
/^__is_bitmap_pgno$/d

s/^__ham_c_close$/xxxDBcursor->c_close/
s/^__ham_c_del$/xxxDBcursor->c_del/
s/^__ham_c_get$/xxxDBcursor->c_get/
s/^__ham_c_put$/xxxDBcursor->c_put/
s/^__ham_cursor$/xxxDB->cursor/
s/^__ham_delete$/xxxDB->del/
s/^__ham_get$/xxxDB->get/
s/^__ham_put$/xxxDB->put/
s/^__ham_stat$/xxxDB->stat/
s/^__ham_sync$/xxxDB->sync/

# Recno.
/^__ram_fmap$/s/.*/__ram_fmap/
/^rp->re_irec$/s/.*/__ram_fmap\
__ram_vmap/
/^re_irec$/s/.*/__ram_fmap\
__ram_vmap/

s/^__ram_c_close$/xxxDBcursor->c_close/
s/^__ram_c_del$/xxxDBcursor->c_del/
s/^__ram_c_get$/xxxDBcursor->c_get/
s/^__ram_c_put$/xxxDBcursor->c_put/
s/^__ram_cursor$/xxxDB->cursor/
s/^__ram_delete$/xxxDB->del/
s/^__ram_get$/xxxDB->get/
s/^__ram_put$/xxxDB->put/
s/^__ram_stat$/xxxDB->stat/
s/^__ram_sync$/xxxDB->sync/

# Log.
/^cmpfunc$/d

# Mpool.
s/^mpreg->pgin$/xxxDBmemp->pgin/
s/^mpreg->pgout$/xxxDBmemp->pgout/

# Txn.
/^mgr->recover$/s/.*/xxxDBenv->tx_recover/

# Db jump table entries.
s/^__db_calloc/calloc/
s/^__db_close$/close/
s/^__db_dirfree/free/
s/^__db_dirlist/opendir\
readdir/
s/^__db_exists$/stat/
s/^__db_free$/free/
s/^__db_ioinfo$/stat/
s/^__db_malloc$/malloc/
s/^__db_map$/mmap/
s/^__db_realloc$/realloc/
s/^__db_seek$/lseek/
s/^__db_strdup$/strdup/
s/^__db_unmap$/munmap/
/^__db_yield$/d
s/^__os_close/close/
s/^__os_fsync/fsync/
s/^__os_open/open/
s/^__os_read/read/
s/^__os_unlink/unlink/
s/^__os_write/write/
