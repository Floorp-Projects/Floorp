# Microsoft Developer Studio Generated NMAKE File, Format Version 4.20
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

!IF "$(CFG)" == ""
CFG=DB_DLL - Win32 Debug
!MESSAGE No configuration specified.  Defaulting to DB_DLL - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "DB_DLL - Win32 Release" && "$(CFG)" != "DB_DLL - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "DB_DLL.mak" CFG="DB_DLL - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "DB_DLL - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "DB_DLL - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 
################################################################################
# Begin Project
# PROP Target_Last_Scanned "DB_DLL - Win32 Debug"
CPP=cl.exe
MTL=mktyplib.exe
RSC=rc.exe

!IF  "$(CFG)" == "DB_DLL - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "DB_DLL__"
# PROP BASE Intermediate_Dir "DB_DLL__"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
OUTDIR=.\Release
INTDIR=.\Release

ALL : "$(OUTDIR)\libdb.dll"

CLEAN : 
	-@erase "$(INTDIR)\bt_close.obj"
	-@erase "$(INTDIR)\bt_compare.obj"
	-@erase "$(INTDIR)\bt_conv.obj"
	-@erase "$(INTDIR)\bt_cursor.obj"
	-@erase "$(INTDIR)\bt_delete.obj"
	-@erase "$(INTDIR)\bt_open.obj"
	-@erase "$(INTDIR)\bt_page.obj"
	-@erase "$(INTDIR)\bt_put.obj"
	-@erase "$(INTDIR)\bt_rec.obj"
	-@erase "$(INTDIR)\bt_recno.obj"
	-@erase "$(INTDIR)\bt_rsearch.obj"
	-@erase "$(INTDIR)\bt_search.obj"
	-@erase "$(INTDIR)\bt_split.obj"
	-@erase "$(INTDIR)\bt_stat.obj"
	-@erase "$(INTDIR)\btree_auto.obj"
	-@erase "$(INTDIR)\cxx_app.obj"
	-@erase "$(INTDIR)\cxx_except.obj"
	-@erase "$(INTDIR)\cxx_lock.obj"
	-@erase "$(INTDIR)\cxx_log.obj"
	-@erase "$(INTDIR)\cxx_mpool.obj"
	-@erase "$(INTDIR)\cxx_table.obj"
	-@erase "$(INTDIR)\cxx_txn.obj"
	-@erase "$(INTDIR)\db.obj"
	-@erase "$(INTDIR)\db_appinit.obj"
	-@erase "$(INTDIR)\db_apprec.obj"
	-@erase "$(INTDIR)\db_auto.obj"
	-@erase "$(INTDIR)\db_byteorder.obj"
	-@erase "$(INTDIR)\db_conv.obj"
	-@erase "$(INTDIR)\db_dispatch.obj"
	-@erase "$(INTDIR)\db_dup.obj"
	-@erase "$(INTDIR)\db_err.obj"
	-@erase "$(INTDIR)\db_log2.obj"
	-@erase "$(INTDIR)\db_overflow.obj"
	-@erase "$(INTDIR)\db_pr.obj"
	-@erase "$(INTDIR)\db_rec.obj"
	-@erase "$(INTDIR)\db_region.obj"
	-@erase "$(INTDIR)\db_ret.obj"
	-@erase "$(INTDIR)\db_salloc.obj"
	-@erase "$(INTDIR)\db_shash.obj"
	-@erase "$(INTDIR)\db_thread.obj"
	-@erase "$(INTDIR)\dbm.obj"
	-@erase "$(INTDIR)\hash.obj"
	-@erase "$(INTDIR)\hash_auto.obj"
	-@erase "$(INTDIR)\hash_conv.obj"
	-@erase "$(INTDIR)\hash_debug.obj"
	-@erase "$(INTDIR)\hash_dup.obj"
	-@erase "$(INTDIR)\hash_func.obj"
	-@erase "$(INTDIR)\hash_page.obj"
	-@erase "$(INTDIR)\hash_rec.obj"
	-@erase "$(INTDIR)\hash_stat.obj"
	-@erase "$(INTDIR)\lock.obj"
	-@erase "$(INTDIR)\lock_conflict.obj"
	-@erase "$(INTDIR)\lock_deadlock.obj"
	-@erase "$(INTDIR)\lock_util.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_archive.obj"
	-@erase "$(INTDIR)\log_auto.obj"
	-@erase "$(INTDIR)\log_compare.obj"
	-@erase "$(INTDIR)\log_findckp.obj"
	-@erase "$(INTDIR)\log_get.obj"
	-@erase "$(INTDIR)\log_put.obj"
	-@erase "$(INTDIR)\log_rec.obj"
	-@erase "$(INTDIR)\log_register.obj"
	-@erase "$(INTDIR)\mp_bh.obj"
	-@erase "$(INTDIR)\mp_fget.obj"
	-@erase "$(INTDIR)\mp_fopen.obj"
	-@erase "$(INTDIR)\mp_fput.obj"
	-@erase "$(INTDIR)\mp_fset.obj"
	-@erase "$(INTDIR)\mp_open.obj"
	-@erase "$(INTDIR)\mp_pr.obj"
	-@erase "$(INTDIR)\mp_region.obj"
	-@erase "$(INTDIR)\mp_sync.obj"
	-@erase "$(INTDIR)\mutex.obj"
	-@erase "$(INTDIR)\os_abs.obj"
	-@erase "$(INTDIR)\os_alloc.obj"
	-@erase "$(INTDIR)\os_config.obj"
	-@erase "$(INTDIR)\os_dir.obj"
	-@erase "$(INTDIR)\os_fid.obj"
	-@erase "$(INTDIR)\os_fsync.obj"
	-@erase "$(INTDIR)\os_map.obj"
	-@erase "$(INTDIR)\os_oflags.obj"
	-@erase "$(INTDIR)\os_open.obj"
	-@erase "$(INTDIR)\os_rpath.obj"
	-@erase "$(INTDIR)\os_rw.obj"
	-@erase "$(INTDIR)\os_seek.obj"
	-@erase "$(INTDIR)\os_sleep.obj"
	-@erase "$(INTDIR)\os_spin.obj"
	-@erase "$(INTDIR)\os_stat.obj"
	-@erase "$(INTDIR)\os_unlink.obj"
	-@erase "$(INTDIR)\strsep.obj"
	-@erase "$(INTDIR)\txn.obj"
	-@erase "$(INTDIR)\txn_auto.obj"
	-@erase "$(INTDIR)\txn_rec.obj"
	-@erase "$(OUTDIR)\libdb.dll"
	-@erase "$(OUTDIR)\libdb.exp"
	-@erase "$(OUTDIR)\libdb.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "." /I "../include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "DB_CREATE_DLL" /YX /c
CPP_PROJ=/nologo /MD /W3 /GX /O2 /Ob2 /I "." /I "../include" /D "NDEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "DB_CREATE_DLL" /Fp"$(INTDIR)/DB_DLL.pch" /YX\
 /Fo"$(INTDIR)/" /c 
CPP_OBJS=.\Release/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
MTL_PROJ=/nologo /D "NDEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/DB_DLL.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /base:0x13000000 /subsystem:windows /dll /machine:I386 /out:"Release/libdb.dll"
LINK32_FLAGS=/nologo /base:0x13000000 /subsystem:windows /dll /incremental:no\
 /pdb:"$(OUTDIR)/libdb.pdb" /machine:I386 /out:"$(OUTDIR)/libdb.dll"\
 /implib:"$(OUTDIR)/libdb.lib" 
LINK32_OBJS= \
	"$(INTDIR)\bt_close.obj" \
	"$(INTDIR)\bt_compare.obj" \
	"$(INTDIR)\bt_conv.obj" \
	"$(INTDIR)\bt_cursor.obj" \
	"$(INTDIR)\bt_delete.obj" \
	"$(INTDIR)\bt_open.obj" \
	"$(INTDIR)\bt_page.obj" \
	"$(INTDIR)\bt_put.obj" \
	"$(INTDIR)\bt_rec.obj" \
	"$(INTDIR)\bt_recno.obj" \
	"$(INTDIR)\bt_rsearch.obj" \
	"$(INTDIR)\bt_search.obj" \
	"$(INTDIR)\bt_split.obj" \
	"$(INTDIR)\bt_stat.obj" \
	"$(INTDIR)\btree_auto.obj" \
	"$(INTDIR)\cxx_app.obj" \
	"$(INTDIR)\cxx_except.obj" \
	"$(INTDIR)\cxx_lock.obj" \
	"$(INTDIR)\cxx_log.obj" \
	"$(INTDIR)\cxx_mpool.obj" \
	"$(INTDIR)\cxx_table.obj" \
	"$(INTDIR)\cxx_txn.obj" \
	"$(INTDIR)\db.obj" \
	"$(INTDIR)\db_appinit.obj" \
	"$(INTDIR)\db_apprec.obj" \
	"$(INTDIR)\db_auto.obj" \
	"$(INTDIR)\db_byteorder.obj" \
	"$(INTDIR)\db_conv.obj" \
	"$(INTDIR)\db_dispatch.obj" \
	"$(INTDIR)\db_dup.obj" \
	"$(INTDIR)\db_err.obj" \
	"$(INTDIR)\db_log2.obj" \
	"$(INTDIR)\db_overflow.obj" \
	"$(INTDIR)\db_pr.obj" \
	"$(INTDIR)\db_rec.obj" \
	"$(INTDIR)\db_region.obj" \
	"$(INTDIR)\db_ret.obj" \
	"$(INTDIR)\db_salloc.obj" \
	"$(INTDIR)\db_shash.obj" \
	"$(INTDIR)\db_thread.obj" \
	"$(INTDIR)\dbm.obj" \
	"$(INTDIR)\hash.obj" \
	"$(INTDIR)\hash_auto.obj" \
	"$(INTDIR)\hash_conv.obj" \
	"$(INTDIR)\hash_debug.obj" \
	"$(INTDIR)\hash_dup.obj" \
	"$(INTDIR)\hash_func.obj" \
	"$(INTDIR)\hash_page.obj" \
	"$(INTDIR)\hash_rec.obj" \
	"$(INTDIR)\hash_stat.obj" \
	"$(INTDIR)\lock.obj" \
	"$(INTDIR)\lock_conflict.obj" \
	"$(INTDIR)\lock_deadlock.obj" \
	"$(INTDIR)\lock_util.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_archive.obj" \
	"$(INTDIR)\log_auto.obj" \
	"$(INTDIR)\log_compare.obj" \
	"$(INTDIR)\log_findckp.obj" \
	"$(INTDIR)\log_get.obj" \
	"$(INTDIR)\log_put.obj" \
	"$(INTDIR)\log_rec.obj" \
	"$(INTDIR)\log_register.obj" \
	"$(INTDIR)\mp_bh.obj" \
	"$(INTDIR)\mp_fget.obj" \
	"$(INTDIR)\mp_fopen.obj" \
	"$(INTDIR)\mp_fput.obj" \
	"$(INTDIR)\mp_fset.obj" \
	"$(INTDIR)\mp_open.obj" \
	"$(INTDIR)\mp_pr.obj" \
	"$(INTDIR)\mp_region.obj" \
	"$(INTDIR)\mp_sync.obj" \
	"$(INTDIR)\mutex.obj" \
	"$(INTDIR)\os_abs.obj" \
	"$(INTDIR)\os_alloc.obj" \
	"$(INTDIR)\os_config.obj" \
	"$(INTDIR)\os_dir.obj" \
	"$(INTDIR)\os_fid.obj" \
	"$(INTDIR)\os_fsync.obj" \
	"$(INTDIR)\os_map.obj" \
	"$(INTDIR)\os_oflags.obj" \
	"$(INTDIR)\os_open.obj" \
	"$(INTDIR)\os_rpath.obj" \
	"$(INTDIR)\os_rw.obj" \
	"$(INTDIR)\os_seek.obj" \
	"$(INTDIR)\os_sleep.obj" \
	"$(INTDIR)\os_spin.obj" \
	"$(INTDIR)\os_stat.obj" \
	"$(INTDIR)\os_unlink.obj" \
	"$(INTDIR)\strsep.obj" \
	"$(INTDIR)\txn.obj" \
	"$(INTDIR)\txn_auto.obj" \
	"$(INTDIR)\txn_rec.obj"

"$(OUTDIR)\libdb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "DB_DLL - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DB_DLL_0"
# PROP BASE Intermediate_Dir "DB_DLL_0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "$(OUTDIR)\libdb.dll"

CLEAN : 
	-@erase "$(INTDIR)\bt_close.obj"
	-@erase "$(INTDIR)\bt_compare.obj"
	-@erase "$(INTDIR)\bt_conv.obj"
	-@erase "$(INTDIR)\bt_cursor.obj"
	-@erase "$(INTDIR)\bt_delete.obj"
	-@erase "$(INTDIR)\bt_open.obj"
	-@erase "$(INTDIR)\bt_page.obj"
	-@erase "$(INTDIR)\bt_put.obj"
	-@erase "$(INTDIR)\bt_rec.obj"
	-@erase "$(INTDIR)\bt_recno.obj"
	-@erase "$(INTDIR)\bt_rsearch.obj"
	-@erase "$(INTDIR)\bt_search.obj"
	-@erase "$(INTDIR)\bt_split.obj"
	-@erase "$(INTDIR)\bt_stat.obj"
	-@erase "$(INTDIR)\btree_auto.obj"
	-@erase "$(INTDIR)\cxx_app.obj"
	-@erase "$(INTDIR)\cxx_except.obj"
	-@erase "$(INTDIR)\cxx_lock.obj"
	-@erase "$(INTDIR)\cxx_log.obj"
	-@erase "$(INTDIR)\cxx_mpool.obj"
	-@erase "$(INTDIR)\cxx_table.obj"
	-@erase "$(INTDIR)\cxx_txn.obj"
	-@erase "$(INTDIR)\db.obj"
	-@erase "$(INTDIR)\db_appinit.obj"
	-@erase "$(INTDIR)\db_apprec.obj"
	-@erase "$(INTDIR)\db_auto.obj"
	-@erase "$(INTDIR)\db_byteorder.obj"
	-@erase "$(INTDIR)\db_conv.obj"
	-@erase "$(INTDIR)\db_dispatch.obj"
	-@erase "$(INTDIR)\db_dup.obj"
	-@erase "$(INTDIR)\db_err.obj"
	-@erase "$(INTDIR)\db_log2.obj"
	-@erase "$(INTDIR)\db_overflow.obj"
	-@erase "$(INTDIR)\db_pr.obj"
	-@erase "$(INTDIR)\db_rec.obj"
	-@erase "$(INTDIR)\db_region.obj"
	-@erase "$(INTDIR)\db_ret.obj"
	-@erase "$(INTDIR)\db_salloc.obj"
	-@erase "$(INTDIR)\db_shash.obj"
	-@erase "$(INTDIR)\db_thread.obj"
	-@erase "$(INTDIR)\dbm.obj"
	-@erase "$(INTDIR)\hash.obj"
	-@erase "$(INTDIR)\hash_auto.obj"
	-@erase "$(INTDIR)\hash_conv.obj"
	-@erase "$(INTDIR)\hash_debug.obj"
	-@erase "$(INTDIR)\hash_dup.obj"
	-@erase "$(INTDIR)\hash_func.obj"
	-@erase "$(INTDIR)\hash_page.obj"
	-@erase "$(INTDIR)\hash_rec.obj"
	-@erase "$(INTDIR)\hash_stat.obj"
	-@erase "$(INTDIR)\lock.obj"
	-@erase "$(INTDIR)\lock_conflict.obj"
	-@erase "$(INTDIR)\lock_deadlock.obj"
	-@erase "$(INTDIR)\lock_util.obj"
	-@erase "$(INTDIR)\log.obj"
	-@erase "$(INTDIR)\log_archive.obj"
	-@erase "$(INTDIR)\log_auto.obj"
	-@erase "$(INTDIR)\log_compare.obj"
	-@erase "$(INTDIR)\log_findckp.obj"
	-@erase "$(INTDIR)\log_get.obj"
	-@erase "$(INTDIR)\log_put.obj"
	-@erase "$(INTDIR)\log_rec.obj"
	-@erase "$(INTDIR)\log_register.obj"
	-@erase "$(INTDIR)\mp_bh.obj"
	-@erase "$(INTDIR)\mp_fget.obj"
	-@erase "$(INTDIR)\mp_fopen.obj"
	-@erase "$(INTDIR)\mp_fput.obj"
	-@erase "$(INTDIR)\mp_fset.obj"
	-@erase "$(INTDIR)\mp_open.obj"
	-@erase "$(INTDIR)\mp_pr.obj"
	-@erase "$(INTDIR)\mp_region.obj"
	-@erase "$(INTDIR)\mp_sync.obj"
	-@erase "$(INTDIR)\mutex.obj"
	-@erase "$(INTDIR)\os_abs.obj"
	-@erase "$(INTDIR)\os_alloc.obj"
	-@erase "$(INTDIR)\os_config.obj"
	-@erase "$(INTDIR)\os_dir.obj"
	-@erase "$(INTDIR)\os_fid.obj"
	-@erase "$(INTDIR)\os_fsync.obj"
	-@erase "$(INTDIR)\os_map.obj"
	-@erase "$(INTDIR)\os_oflags.obj"
	-@erase "$(INTDIR)\os_open.obj"
	-@erase "$(INTDIR)\os_rpath.obj"
	-@erase "$(INTDIR)\os_rw.obj"
	-@erase "$(INTDIR)\os_seek.obj"
	-@erase "$(INTDIR)\os_sleep.obj"
	-@erase "$(INTDIR)\os_spin.obj"
	-@erase "$(INTDIR)\os_stat.obj"
	-@erase "$(INTDIR)\os_unlink.obj"
	-@erase "$(INTDIR)\strsep.obj"
	-@erase "$(INTDIR)\txn.obj"
	-@erase "$(INTDIR)\txn_auto.obj"
	-@erase "$(INTDIR)\txn_rec.obj"
	-@erase "$(INTDIR)\vc40.idb"
	-@erase "$(INTDIR)\vc40.pdb"
	-@erase "$(OUTDIR)\libdb.dll"
	-@erase "$(OUTDIR)\libdb.exp"
	-@erase "$(OUTDIR)\libdb.ilk"
	-@erase "$(OUTDIR)\libdb.lib"
	-@erase "$(OUTDIR)\libdb.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I "../include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "DB_CREATE_DLL" /YX /c
CPP_PROJ=/nologo /MDd /W3 /Gm /GX /Zi /Od /I "." /I "../include" /D "_DEBUG" /D\
 "WIN32" /D "_WINDOWS" /D "DB_CREATE_DLL" /Fp"$(INTDIR)/DB_DLL.pch" /YX\
 /Fo"$(INTDIR)/" /Fd"$(INTDIR)/" /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.\.
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
MTL_PROJ=/nologo /D "_DEBUG" /win32 
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o"$(OUTDIR)/DB_DLL.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /dll /debug /machine:I386
# ADD LINK32 /nologo /base:0x13000000 /subsystem:windows /dll /debug /machine:I386 /out:"Debug/libdb.dll"
LINK32_FLAGS=/nologo /base:0x13000000 /subsystem:windows /dll /incremental:yes\
 /pdb:"$(OUTDIR)/libdb.pdb" /debug /machine:I386 /out:"$(OUTDIR)/libdb.dll"\
 /implib:"$(OUTDIR)/libdb.lib" 
LINK32_OBJS= \
	"$(INTDIR)\bt_close.obj" \
	"$(INTDIR)\bt_compare.obj" \
	"$(INTDIR)\bt_conv.obj" \
	"$(INTDIR)\bt_cursor.obj" \
	"$(INTDIR)\bt_delete.obj" \
	"$(INTDIR)\bt_open.obj" \
	"$(INTDIR)\bt_page.obj" \
	"$(INTDIR)\bt_put.obj" \
	"$(INTDIR)\bt_rec.obj" \
	"$(INTDIR)\bt_recno.obj" \
	"$(INTDIR)\bt_rsearch.obj" \
	"$(INTDIR)\bt_search.obj" \
	"$(INTDIR)\bt_split.obj" \
	"$(INTDIR)\bt_stat.obj" \
	"$(INTDIR)\btree_auto.obj" \
	"$(INTDIR)\cxx_app.obj" \
	"$(INTDIR)\cxx_except.obj" \
	"$(INTDIR)\cxx_lock.obj" \
	"$(INTDIR)\cxx_log.obj" \
	"$(INTDIR)\cxx_mpool.obj" \
	"$(INTDIR)\cxx_table.obj" \
	"$(INTDIR)\cxx_txn.obj" \
	"$(INTDIR)\db.obj" \
	"$(INTDIR)\db_appinit.obj" \
	"$(INTDIR)\db_apprec.obj" \
	"$(INTDIR)\db_auto.obj" \
	"$(INTDIR)\db_byteorder.obj" \
	"$(INTDIR)\db_conv.obj" \
	"$(INTDIR)\db_dispatch.obj" \
	"$(INTDIR)\db_dup.obj" \
	"$(INTDIR)\db_err.obj" \
	"$(INTDIR)\db_log2.obj" \
	"$(INTDIR)\db_overflow.obj" \
	"$(INTDIR)\db_pr.obj" \
	"$(INTDIR)\db_rec.obj" \
	"$(INTDIR)\db_region.obj" \
	"$(INTDIR)\db_ret.obj" \
	"$(INTDIR)\db_salloc.obj" \
	"$(INTDIR)\db_shash.obj" \
	"$(INTDIR)\db_thread.obj" \
	"$(INTDIR)\dbm.obj" \
	"$(INTDIR)\hash.obj" \
	"$(INTDIR)\hash_auto.obj" \
	"$(INTDIR)\hash_conv.obj" \
	"$(INTDIR)\hash_debug.obj" \
	"$(INTDIR)\hash_dup.obj" \
	"$(INTDIR)\hash_func.obj" \
	"$(INTDIR)\hash_page.obj" \
	"$(INTDIR)\hash_rec.obj" \
	"$(INTDIR)\hash_stat.obj" \
	"$(INTDIR)\lock.obj" \
	"$(INTDIR)\lock_conflict.obj" \
	"$(INTDIR)\lock_deadlock.obj" \
	"$(INTDIR)\lock_util.obj" \
	"$(INTDIR)\log.obj" \
	"$(INTDIR)\log_archive.obj" \
	"$(INTDIR)\log_auto.obj" \
	"$(INTDIR)\log_compare.obj" \
	"$(INTDIR)\log_findckp.obj" \
	"$(INTDIR)\log_get.obj" \
	"$(INTDIR)\log_put.obj" \
	"$(INTDIR)\log_rec.obj" \
	"$(INTDIR)\log_register.obj" \
	"$(INTDIR)\mp_bh.obj" \
	"$(INTDIR)\mp_fget.obj" \
	"$(INTDIR)\mp_fopen.obj" \
	"$(INTDIR)\mp_fput.obj" \
	"$(INTDIR)\mp_fset.obj" \
	"$(INTDIR)\mp_open.obj" \
	"$(INTDIR)\mp_pr.obj" \
	"$(INTDIR)\mp_region.obj" \
	"$(INTDIR)\mp_sync.obj" \
	"$(INTDIR)\mutex.obj" \
	"$(INTDIR)\os_abs.obj" \
	"$(INTDIR)\os_alloc.obj" \
	"$(INTDIR)\os_config.obj" \
	"$(INTDIR)\os_dir.obj" \
	"$(INTDIR)\os_fid.obj" \
	"$(INTDIR)\os_fsync.obj" \
	"$(INTDIR)\os_map.obj" \
	"$(INTDIR)\os_oflags.obj" \
	"$(INTDIR)\os_open.obj" \
	"$(INTDIR)\os_rpath.obj" \
	"$(INTDIR)\os_rw.obj" \
	"$(INTDIR)\os_seek.obj" \
	"$(INTDIR)\os_sleep.obj" \
	"$(INTDIR)\os_spin.obj" \
	"$(INTDIR)\os_stat.obj" \
	"$(INTDIR)\os_unlink.obj" \
	"$(INTDIR)\strsep.obj" \
	"$(INTDIR)\txn.obj" \
	"$(INTDIR)\txn_auto.obj" \
	"$(INTDIR)\txn_rec.obj"

"$(OUTDIR)\libdb.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.c{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_SBRS)}.sbr:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Target

# Name "DB_DLL - Win32 Release"
# Name "DB_DLL - Win32 Debug"

!IF  "$(CFG)" == "DB_DLL - Win32 Release"

!ELSEIF  "$(CFG)" == "DB_DLL - Win32 Debug"

!ENDIF 

################################################################################
# Begin Source File

SOURCE=\db\btree\btree_auto.c
DEP_CPP_BTREE=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\btree_auto.obj" : $(SOURCE) $(DEP_CPP_BTREE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_compare.c
DEP_CPP_BT_CO=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_compare.obj" : $(SOURCE) $(DEP_CPP_BT_CO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_conv.c
DEP_CPP_BT_CON=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_conv.obj" : $(SOURCE) $(DEP_CPP_BT_CON) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_cursor.c
DEP_CPP_BT_CU=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_cursor.obj" : $(SOURCE) $(DEP_CPP_BT_CU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_delete.c
DEP_CPP_BT_DE=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_delete.obj" : $(SOURCE) $(DEP_CPP_BT_DE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_open.c
DEP_CPP_BT_OP=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_open.obj" : $(SOURCE) $(DEP_CPP_BT_OP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_page.c
DEP_CPP_BT_PA=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_page.obj" : $(SOURCE) $(DEP_CPP_BT_PA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_put.c
DEP_CPP_BT_PU=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_put.obj" : $(SOURCE) $(DEP_CPP_BT_PU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_rec.c
DEP_CPP_BT_RE=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_rec.obj" : $(SOURCE) $(DEP_CPP_BT_RE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_rsearch.c
DEP_CPP_BT_RS=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_rsearch.obj" : $(SOURCE) $(DEP_CPP_BT_RS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_search.c
DEP_CPP_BT_SE=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_search.obj" : $(SOURCE) $(DEP_CPP_BT_SE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_split.c
DEP_CPP_BT_SP=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_split.obj" : $(SOURCE) $(DEP_CPP_BT_SP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_stat.c
DEP_CPP_BT_ST=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_stat.obj" : $(SOURCE) $(DEP_CPP_BT_ST) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_close.c
DEP_CPP_BT_CL=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_close.obj" : $(SOURCE) $(DEP_CPP_BT_CL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_txn.cpp
DEP_CPP_CXX_T=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_txn.obj" : $(SOURCE) $(DEP_CPP_CXX_T) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_except.cpp
DEP_CPP_CXX_E=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_except.obj" : $(SOURCE) $(DEP_CPP_CXX_E) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_lock.cpp
DEP_CPP_CXX_L=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_lock.obj" : $(SOURCE) $(DEP_CPP_CXX_L) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_log.cpp
DEP_CPP_CXX_LO=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_log.obj" : $(SOURCE) $(DEP_CPP_CXX_LO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_mpool.cpp
DEP_CPP_CXX_M=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_mpool.obj" : $(SOURCE) $(DEP_CPP_CXX_M) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_table.cpp
DEP_CPP_CXX_TA=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_table.obj" : $(SOURCE) $(DEP_CPP_CXX_TA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\cxx\cxx_app.cpp
DEP_CPP_CXX_A=\
	".\../include\cxx_int.h"\
	".\../include\db_cxx.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\cxx_app.obj" : $(SOURCE) $(DEP_CPP_CXX_A) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_thread.c
DEP_CPP_DB_TH=\
	"..\include\db_auto.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_thread.obj" : $(SOURCE) $(DEP_CPP_DB_TH) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_auto.c
DEP_CPP_DB_AU=\
	"..\include\db_auto.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_auto.obj" : $(SOURCE) $(DEP_CPP_DB_AU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_conv.c
DEP_CPP_DB_CO=\
	"..\include\db_auto.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_conv.obj" : $(SOURCE) $(DEP_CPP_DB_CO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_dispatch.c
DEP_CPP_DB_DI=\
	"..\include\db_auto.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_dispatch.obj" : $(SOURCE) $(DEP_CPP_DB_DI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_dup.c
DEP_CPP_DB_DU=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_dup.obj" : $(SOURCE) $(DEP_CPP_DB_DU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_overflow.c
DEP_CPP_DB_OV=\
	"..\include\db_auto.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_overflow.obj" : $(SOURCE) $(DEP_CPP_DB_OV) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_pr.c
DEP_CPP_DB_PR=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_pr.obj" : $(SOURCE) $(DEP_CPP_DB_PR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_rec.c
DEP_CPP_DB_RE=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_rec.obj" : $(SOURCE) $(DEP_CPP_DB_RE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db_ret.c
DEP_CPP_DB_RET=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_ret.obj" : $(SOURCE) $(DEP_CPP_DB_RET) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\db\db.c
DEP_CPP_DB_C3c=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\mp_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_shash.h"\
	".\../include\db_swap.h"\
	".\../include\hash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db.obj" : $(SOURCE) $(DEP_CPP_DB_C3c) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_shash.c
DEP_CPP_DB_SH=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_shash.obj" : $(SOURCE) $(DEP_CPP_DB_SH) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_apprec.c
DEP_CPP_DB_AP=\
	"..\include\db_auto.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	"..\include\txn_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_apprec.obj" : $(SOURCE) $(DEP_CPP_DB_AP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_byteorder.c
DEP_CPP_DB_BY=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_byteorder.obj" : $(SOURCE) $(DEP_CPP_DB_BY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_err.c
DEP_CPP_DB_ER=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_err.obj" : $(SOURCE) $(DEP_CPP_DB_ER) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_log2.c
DEP_CPP_DB_LO=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_log2.obj" : $(SOURCE) $(DEP_CPP_DB_LO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_region.c
DEP_CPP_DB_REG=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_region.obj" : $(SOURCE) $(DEP_CPP_DB_REG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_salloc.c
DEP_CPP_DB_SA=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_salloc.obj" : $(SOURCE) $(DEP_CPP_DB_SA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\common\db_appinit.c
DEP_CPP_DB_APP=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	"..\include\txn_ext.h"\
	".\../include\btree.h"\
	".\../include\clib_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\db_appinit.obj" : $(SOURCE) $(DEP_CPP_DB_APP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\dbm\dbm.c
DEP_CPP_DBM_C=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\dbm.obj" : $(SOURCE) $(DEP_CPP_DBM_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_stat.c
DEP_CPP_HASH_=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_stat.obj" : $(SOURCE) $(DEP_CPP_HASH_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_auto.c
DEP_CPP_HASH_A=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_auto.obj" : $(SOURCE) $(DEP_CPP_HASH_A) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_conv.c
DEP_CPP_HASH_C=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_conv.obj" : $(SOURCE) $(DEP_CPP_HASH_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_debug.c
DEP_CPP_HASH_D=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_debug.obj" : $(SOURCE) $(DEP_CPP_HASH_D) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_dup.c
DEP_CPP_HASH_DU=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_dup.obj" : $(SOURCE) $(DEP_CPP_HASH_DU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_func.c
DEP_CPP_HASH_F=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_func.obj" : $(SOURCE) $(DEP_CPP_HASH_F) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_page.c
DEP_CPP_HASH_P=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_swap.h"\
	".\../include\hash.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_page.obj" : $(SOURCE) $(DEP_CPP_HASH_P) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash_rec.c
DEP_CPP_HASH_R=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash_rec.obj" : $(SOURCE) $(DEP_CPP_HASH_R) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\hash\hash.c
DEP_CPP_HASH_C60=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\hash.obj" : $(SOURCE) $(DEP_CPP_HASH_C60) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\lock\lock_util.c
DEP_CPP_LOCK_=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\lock_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_shash.h"\
	".\../include\hash.h"\
	".\../include\lock.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\lock_util.obj" : $(SOURCE) $(DEP_CPP_LOCK_) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\lock\lock_conflict.c
DEP_CPP_LOCK_C=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\lock_conflict.obj" : $(SOURCE) $(DEP_CPP_LOCK_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\lock\lock_deadlock.c
DEP_CPP_LOCK_D=\
	"..\include\lock_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\lock.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\lock_deadlock.obj" : $(SOURCE) $(DEP_CPP_LOCK_D) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\lock\lock.c
DEP_CPP_LOCK_C68=\
	"..\include\db_auto.h"\
	"..\include\lock_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_shash.h"\
	".\../include\lock.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\lock.obj" : $(SOURCE) $(DEP_CPP_LOCK_C68) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_register.c
DEP_CPP_LOG_R=\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_register.obj" : $(SOURCE) $(DEP_CPP_LOG_R) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_archive.c
DEP_CPP_LOG_A=\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\clib_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_dispatch.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_archive.obj" : $(SOURCE) $(DEP_CPP_LOG_A) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_auto.c
DEP_CPP_LOG_AU=\
	"..\include\db_auto.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_auto.obj" : $(SOURCE) $(DEP_CPP_LOG_AU) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_compare.c
DEP_CPP_LOG_C=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_compare.obj" : $(SOURCE) $(DEP_CPP_LOG_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_findckp.c
DEP_CPP_LOG_F=\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	"..\include\txn_ext.h"\
	".\../include\common_ext.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_findckp.obj" : $(SOURCE) $(DEP_CPP_LOG_F) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_get.c
DEP_CPP_LOG_G=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_get.obj" : $(SOURCE) $(DEP_CPP_LOG_G) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_put.c
DEP_CPP_LOG_P=\
	"..\include\db_auto.h"\
	"..\include\hash_auto.h"\
	"..\include\hash_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\hash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_put.obj" : $(SOURCE) $(DEP_CPP_LOG_P) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log_rec.c
DEP_CPP_LOG_RE=\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_dispatch.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log_rec.obj" : $(SOURCE) $(DEP_CPP_LOG_RE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\log\log.c
DEP_CPP_LOG_C7a=\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_shash.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\log.obj" : $(SOURCE) $(DEP_CPP_LOG_C7a) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_sync.c
DEP_CPP_MP_SY=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_sync.obj" : $(SOURCE) $(DEP_CPP_MP_SY) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_fget.c
DEP_CPP_MP_FG=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_fget.obj" : $(SOURCE) $(DEP_CPP_MP_FG) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_fopen.c
DEP_CPP_MP_FO=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_fopen.obj" : $(SOURCE) $(DEP_CPP_MP_FO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_fput.c
DEP_CPP_MP_FP=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_fput.obj" : $(SOURCE) $(DEP_CPP_MP_FP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_fset.c
DEP_CPP_MP_FS=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_fset.obj" : $(SOURCE) $(DEP_CPP_MP_FS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_open.c
DEP_CPP_MP_OP=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_open.obj" : $(SOURCE) $(DEP_CPP_MP_OP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_pr.c
DEP_CPP_MP_PR=\
	"..\include\mp_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_pr.obj" : $(SOURCE) $(DEP_CPP_MP_PR) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_region.c
DEP_CPP_MP_RE=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_region.obj" : $(SOURCE) $(DEP_CPP_MP_RE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mp\mp_bh.c
DEP_CPP_MP_BH=\
	"..\include\mp_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_shash.h"\
	".\../include\mp.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mp_bh.obj" : $(SOURCE) $(DEP_CPP_MP_BH) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\mutex\mutex.c
DEP_CPP_MUTEX=\
	"..\mutex\68020.gcc"\
	"..\mutex\sco.cc"\
	"..\mutex\sparc.gcc"\
	"..\mutex\x86.gcc"\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\mutex.obj" : $(SOURCE) $(DEP_CPP_MUTEX) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\txn\txn_rec.c
DEP_CPP_TXN_R=\
	"..\include\db_auto.h"\
	"..\include\txn_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\txn_rec.obj" : $(SOURCE) $(DEP_CPP_TXN_R) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\txn\txn_auto.c
DEP_CPP_TXN_A=\
	"..\include\db_auto.h"\
	"..\include\txn_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\txn_auto.obj" : $(SOURCE) $(DEP_CPP_TXN_A) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\txn\txn.c
DEP_CPP_TXN_C=\
	"..\include\db_auto.h"\
	"..\include\lock_ext.h"\
	"..\include\log_auto.h"\
	"..\include\log_ext.h"\
	"..\include\txn_ext.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_dispatch.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\db_shash.h"\
	".\../include\lock.h"\
	".\../include\log.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\../include\txn.h"\
	".\../include\txn_auto.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\txn.obj" : $(SOURCE) $(DEP_CPP_TXN_C) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\clib\strsep.c
DEP_CPP_STRSE=\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\strsep.obj" : $(SOURCE) $(DEP_CPP_STRSE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\btree\bt_recno.c
DEP_CPP_BT_REC=\
	"..\include\btree_auto.h"\
	"..\include\btree_ext.h"\
	"..\include\db_auto.h"\
	".\../include\btree.h"\
	".\../include\common_ext.h"\
	".\../include\db_am.h"\
	".\../include\db_ext.h"\
	".\../include\db_page.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\bt_recno.obj" : $(SOURCE) $(DEP_CPP_BT_REC) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_sleep.c
DEP_CPP_OS_SL=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_sleep.obj" : $(SOURCE) $(DEP_CPP_OS_SL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_dir.c
DEP_CPP_OS_DI=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_dir.obj" : $(SOURCE) $(DEP_CPP_OS_DI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_fid.c
DEP_CPP_OS_FI=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_fid.obj" : $(SOURCE) $(DEP_CPP_OS_FI) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_map.c
DEP_CPP_OS_MA=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_map.obj" : $(SOURCE) $(DEP_CPP_OS_MA) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_seek.c
DEP_CPP_OS_SE=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_seek.obj" : $(SOURCE) $(DEP_CPP_OS_SE) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_abs.c
DEP_CPP_OS_AB=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_abs.obj" : $(SOURCE) $(DEP_CPP_OS_AB) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_fsync.c
DEP_CPP_OS_FS=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_fsync.obj" : $(SOURCE) $(DEP_CPP_OS_FS) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_oflags.c
DEP_CPP_OS_OF=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_oflags.obj" : $(SOURCE) $(DEP_CPP_OS_OF) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_open.c
DEP_CPP_OS_OP=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_open.obj" : $(SOURCE) $(DEP_CPP_OS_OP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_rpath.c
DEP_CPP_OS_RP=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_rpath.obj" : $(SOURCE) $(DEP_CPP_OS_RP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_rw.c
DEP_CPP_OS_RW=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_rw.obj" : $(SOURCE) $(DEP_CPP_OS_RW) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_stat.c
DEP_CPP_OS_ST=\
	".\../include\common_ext.h"\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_stat.obj" : $(SOURCE) $(DEP_CPP_OS_ST) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_unlink.c
DEP_CPP_OS_UN=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_unlink.obj" : $(SOURCE) $(DEP_CPP_OS_UN) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_config.c
DEP_CPP_OS_CO=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_config.obj" : $(SOURCE) $(DEP_CPP_OS_CO) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os.win32\os_spin.c
DEP_CPP_OS_SP=\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_spin.obj" : $(SOURCE) $(DEP_CPP_OS_SP) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
################################################################################
# Begin Source File

SOURCE=\db\os\os_alloc.c
DEP_CPP_OS_AL=\
	".\../include\mutex_ext.h"\
	".\../include\os_ext.h"\
	".\../include\os_func.h"\
	".\../include\queue.h"\
	".\../include\shqueue.h"\
	".\config.h"\
	".\db.h"\
	".\db_int.h"\
	{$(INCLUDE)}"\sys\stat.h"\
	{$(INCLUDE)}"\sys\types.h"\
	

"$(INTDIR)\os_alloc.obj" : $(SOURCE) $(DEP_CPP_OS_AL) "$(INTDIR)"
   $(CPP) $(CPP_PROJ) $(SOURCE)


# End Source File
# End Target
# End Project
################################################################################
