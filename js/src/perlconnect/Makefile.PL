use ExtUtils::MakeMaker;

# we suppose running unix and files placed in perlconnect/ under
# js base; otherwise you have to hack the next line

$jsbase = `pwd`;
chomp $jsbase;
$jsbase =~ s|/perlconnect||;


$inc = "-I$jsbase -I$jsbase/Linux_All_DBG.OBJ";
$lib = "-L$jsbase/Linux_All_DBG.OBJ -ljs";


%extras = ();

if ($^O eq "MSWin32") {
    $define = "-DXP_PC";
    $extras{OBJECT} = '$(BASEEXT)$(OBJ_EXT) jsperl.obj';
} else {
    $define = '-DXP_UNIX';
}


WriteMakefile(NAME => 'JS',
	      DEFINE => $define,
	      INC => $inc,
	      LIBS => $lib,
	      VERSION_FROM => 'JS.pm',
	      %extras,);

__END__
