$! Command file to install/deinstall Mozilla image.
$! This command file must exist in the root Mozilla directory (where the main
$! images and shareables reside).
$!
$! P1 = INSTALL command to use.
$!      Default is "ADD /OPEN /HEADER_RESIDENT /SHARE"
$!	To remove previously installed images pass REMOVE as P1.
$!
$ saved_dir = f$environment("default")
$!
$ if p1 .eqs. "" then p1 = "add /open /head /share"
$ me = f$envir("procedure")
$ here = f$parse(me,,,"device") + f$parse(me,,,"directory")
$!
$! Mozilla-Bin
$!
$ call do_one "''p1'" "''here'mozilla-bin."
$!
$! All the .so files in the main directory.
$!
$ call do_many "''p1'" "''here'*.so"
$!
$! All the .so files in the components directory.
$!
$ set default 'here'
$ set default [.components]
$ me = f$envir("default")
$ here = f$parse(me,,,"device") + f$parse(me,,,"directory")
$ call do_many "''p1'" "''here'*.so"
$!
$ set default 'saved_dir'
$ exit
$!
$do_one:
$ subroutine
$ write sys$output "Doing ",p2
$ install 'p1' 'p2'
$ endsubroutine
$!
$!
$do_many:
$ subroutine
$loop:
$ f=f$search(p2)
$ if f .nes. ""
$ then
$   v=f$parse(f,,,"version")
$   f=f-v
$   call do_one "''p1'" "''f'"
$   goto loop
$ endif
$ endsubroutine
