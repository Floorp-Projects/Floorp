$ verify = 'f$verify(0)
$ if p1 .eqs. "DECW" then goto decw
$!
$! This command file in not used by the Mozilla/CSWB software.
$! It is here because if you report a software problem you may be asked
$! to run it and send the output back to the support specialist.
$!
$ set verify
$ set noon
$ show process /quota
$ show display
$ write sys$output f$environment("procedure")
$ show log /ful sys$disk,sys$login,tmp,home
$ show log /ful decc*
$ dir sys$login.;
$ run sys$common:[syshlp.examples.decw.utils]xdpyinfo.exe
$ show system /noprocess
$ ucx show version
$ type sys$sysroot:[sysmgr]decw$private_server_setup.com
$ set noverify
$ @sys$update:decw$versions all
$decw:
$ c = ""
$f_loop:
$ pid = f$pid(c)
$ if pid .eqs. ""
$ then
$   write sys$output "Unable to find DECW$SERVER process"
$   exit
$ endif
$ if f$getjpi(pid,"prcnam") .nes. "DECW$SERVER_0" then goto f_loop
$loop:
$ quota = f$getjpi(pid,"pgflquota")
$ inuse = f$getjpi(pid,"pagfilcnt")
$ write sys$output f$fao("!UL remaining out of !UL at !%D",inuse,quota,0)
$ if verify then set verify
