use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	=> 'Mozilla::LDAP::API',
    'VERSION_FROM' => 'API.pm', # finds $VERSION
    'LIBS'	=> ['-lldapssl30'],   # e.g., '-lm' 
    'DEFINE'	=> '',     # e.g., '-DHAVE_SOMETHING' 
    'INC'	=> '-I/export/source/ldapsdk/include',     # e.g., '-I' 
);
