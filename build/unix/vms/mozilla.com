$! Command file to run Mozilla.
$! This command file must exist in the root Mozilla directory (where the main
$! images and shareables reside).
$!
$ moz_user = f$edit(f$getjpi("","username"),"trim")
$ moz_cwd = f$environment("default")
$ moz_image = f$trnlnm("MOZILLA_IMAGE")
$ if moz_image .eqs. "" then moz_image = "mozilla-bin"
$ moz_image = f$edit(moz_image,"lowercase")
$!
$ moz_self = f$envir("procedure")
$ moz_dir = f$parse(moz_self,,,"device") + f$parse(moz_self,,,"directory")
$!
$ moz_gblpages_needed = (336 * 120/100)
$ moz_gblsects_needed = (1 * 120/100) 
$ moz_gblpages_actual = f$getsyi("free_gblpages")
$ moz_gblsects_actual = f$getsyi("free_gblsects")
$ moz_wait = 0
$ if moz_gblpages_actual .lt. moz_gblpages_needed
$ then
$   write sys$output -
	f$fao("WARNING, estimated global pages needed=!UL, available=!UL",-
			moz_gblpages_needed, moz_gblpages_actual)
$   moz_wait = 1
$ endif
$ if moz_gblsects_actual .lt. moz_gblsects_needed
$ then
$   write sys$output -
	f$fao("WARNING, estimated global sections needed=!UL, available=!UL",-
			moz_gblsects_needed, moz_gblsects_actual)
$   moz_wait = 1
$ endif
$ if moz_wait .and. (f$mode() .eqs. "INTERACTIVE")
$ then
$   type sys$input

    The above system parameter(s) may not be sufficient to
    successfully run Mozilla. 

$   read/prompt="Do you wish to continue [NO]: " sys$command moz_ans
$   if .not. moz_ans then exit
$ endif
$!
$! We need the directory as a unix-style spec.
$!
$ moz_unix = "/" + f$parse(moz_self,,,"device") - ":" 
$ moz_self_dir = f$parse(moz_self,,,"directory") - "[" - "]" - "<" - ">"
$ i=0
$uloop:
$ e=f$element(i,".",moz_self_dir)
$ if e .nes. "."
$ then
$   moz_unix = moz_unix + "/" + e
$   i=i+1
$   goto uloop
$ endif
$ moz_unix = f$edit(moz_unix,"lowercase")
$!
$ moz_found_one = 0
$so_loop:
$ moz_so_file = f$search("''moz_dir'*.so")
$ if moz_so_file .nes. ""
$ then
$   name = f$parse(moz_so_file,,,"name")
$   define /user 'name' 'moz_dir''name'.so
$   moz_found_one = 1
$   goto so_loop
$ endif
$ if .not. moz_found_one
$ then
$   write sys$output "Unable to locate Mozilla images. Most likely reason is"
$   write sys$output "because the protection on the directory"
$   write sys$output moz_dir
$   write sys$output "does not allow you READ access."
$   exit
$ endif
$!
$ ipc_shr = f$trnlnm("ucx$ipc_shr") - ".EXE" + ".EXE"
$ if (f$locate("MULTINET",ipc_shr) .ne. f$length(ipc_shr)) .or. -
     (f$locate("TCPWARE",ipc_shr) .ne. f$length(ipc_shr))
$ then
$    define /user VMS_NULL_DL_NAME 'ipc_shr'
$ else
$    define /user VMS_NULL_DL_NAME SYS$SHARE:TCPIP$IPC_SHR.EXE
$ endif
$ define /user GETIPNODEBYNAME TCPIP$GETIPNODEBYNAME
$ define /user GETIPNODEBYADDR TCPIP$GETIPNODEBYADDR
$ define /user FREEHOSTENT     TCPIP$FREEHOSTENT
$!
$! A networking problem which is still unresolved means that by default
$! the IPv6 support is disabled. If you want to turn it back on define
$! the logical MOZILLA_IPV6 (to anything), but beware that you may 
$! encounter hangs.
$!
$ if f$trnlnm("MOZILLA_IPV6") .eqs. ""
$ then
$   define /user /nolog GETIPNODEBYNAME NO_SUCH_NAME
$ endif
$!
$! These logicals define how files are created/opened.
$! The old code used: "shr=get,put", "rfm=stmlf", "deq=500", "fop=dfw,tef"
$!
$! Executables
$ define /user VMS_OPEN_ARGS_1 ".EXE.SO.SFX_AXPEXE.SFX_VAXEXE", -
	"ctx=stm", "rfm=fix", "rat=none", "mrs=512"
$!
$! VMS savesets
$ define /user VMS_OPEN_ARGS_2 ".BCK.SAV", -
	"ctx=stm", "rfm=fix", "rat=none", "mrs=32256"
$!
$! Binary files. STM doesn't work, needs to be STMLF.
$ define /user VMS_OPEN_ARGS_3 "..DB.GIF.JAR.JPG.MAB.MFASL.MSF.WAV.XPM.XPT", -
	"ctx=stm", "rfm=stmlf", "rat=none"
$!
$! Text files - covered by the catchall
$! ".BAK.COM.CSS.DAT.DTD.HTM.HTML.JS.RDF.NET.ORG.SH.SRC.TBL.TXT.XML.XUL"
$!
$ define /user VMS_OPEN_ARGS_4 " ",-
	"ctx=stm", "rfm=stmlf", "rat=cr"
$!
$ if f$trnlnm("USER") .eqs. "" then define /user user "''moz_user'"
$ if f$trnlnm("LOGNAME") .eqs. "" then define /user logname "''moz_cwd'"
$ define /user MOZILLA_FIVE_HOME "''moz_unix'"
$ define /user VMS_USE_VMS_DEF_PROT 1
$ define /user VMS_ACCESS_FIX_WOK 1
$! define /user VMS_FILE_OPEN_MODE 3
$! define /user VMS_POLL_TIMER_MIN  10
$! define /user VMS_POLL_TIMER_DELTA 0
$! define /user VMS_POLL_TIMER_MAX 200
$! define /user NSPR_LOG_MODULES "all:5"
$! define /user NSPR_LOG_FILE dkb100:[work]log.log
$! define /user VMS_TRACE_FILENAMES 1
$!
$ define /user DECC$EFS_CASE_PRESERVE 0
$!
$! GTK key mapping mode: 0=none, 1=LK, 2=PC
$ if f$trnlnm("GTK_KEY_MAPPING_MODE") .eqs. "" then -
    define /user GTK_KEY_MAPPING_MODE 1
$!
$ write sys$output "Starting ''moz_image'..."
$ mcr 'moz_dir''moz_image'. 'p1' 'p2' 'p3' 'p4' 'p5' 'p6' 'p7' 'p8'
$ set default 'moz_dir'
$ if f$search("[.chrome]*.*;-1") .nes. ""
$ then
$   set default [.chrome]
$   moz_chrome = f$environment("default")
$   purge 'moz_chrome'
$   moz_ver = f$parse(f$search("''moz_chrome'all-skins.rdf"),,,"version") - ";"
$   if moz_ver .gt. 10000 then rename 'moz_chrome'*.rdf *.*;1
$ endif
$!
$ moz_keep = f$trnlnm("MOZILLA_PURGE_KEEP")
$ if moz_keep .eqs. "" then moz_keep = "5"
$ set default sys$login
$ if f$search("[._MOZILLA.*]*.*") .nes. ""
$ then
$   purge /keep='moz_keep' [._mozilla.*...]
$ endif
$ set default 'moz_cwd'
$!
$ exit
