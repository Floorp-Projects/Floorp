use ExtUtils::MakeMaker;

# edit me!
$jsbase = "/home/foo/mozilla/js/src"

$inc = "-I$jsbase -I$jsbase/Linux_All_DBG.OBJ";

$lib = "-L$jsbase/Linux_All_DBG.OBJ -ljs";

WriteMakefile(NAME => 'JS',
	      DEFINE => '-DXP_UNIX',
	      INC => $inc,
	      LIBS => $lib,
	      VERSION_FROM => 'JS.pm');

__END__
